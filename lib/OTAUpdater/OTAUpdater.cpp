#include "OTAUpdater.h"

OTAUpdater::OTAUpdater(DisplayManager* displayManager, HardwareBoardStatus* systemStatus) : 
_displayManager(displayManager), _systemStatus(systemStatus) {
}

void OTAUpdater::begin(AsyncWebServer* server) {
    ElegantOTA.begin(server);
    ElegantOTA.onStart([this]() {
        _systemStatus->setUpdating(true);
        
        // Encender pantalla si está apagada y activar efecto fade
        _systemStatus->setDisplayEnabled(true);
        _displayManager->displayOn();
        _displayManager->startFadeEffect();
        
        _displayManager->clear();
        _displayManager->setFont(ArialMT_Plain_16);
        _displayManager->setTextAlignment(TEXT_ALIGN_CENTER);
        _displayManager->drawString(64, 10, "Actualizando...");
        _displayManager->setFont(ArialMT_Plain_10);
        _displayManager->drawString(64, 35, "No desconecte");
        _displayManager->display();
        
        Serial.println("=== INICIANDO ACTUALIZACIÓN OTA ===");
    });
    ElegantOTA.onProgress([this](size_t current, size_t total) {
        static unsigned long lastUpdate = 0;
        unsigned long now = millis();
        
        if (now - lastUpdate > 100) {
            lastUpdate = now;
            
            int progress = (current * 100) / total;
            
            _displayManager->drawProgressBar(10, 50, 110, 10, progress);
            _displayManager->display();
        }
    });
    ElegantOTA.onEnd([this](bool success) {
        _displayManager->stopFadeEffect();
        _systemStatus->setUpdating(false);
        
        if (success) {
            _displayManager->setLedState(true); // LED fijo durante mensaje final
            _displayManager->clear();
            _displayManager->setFont(ArialMT_Plain_16);
            _displayManager->setTextAlignment(TEXT_ALIGN_CENTER);
            _displayManager->drawString(64, 10, "Actualización");
            _displayManager->drawString(64, 30, "Completada!");
            _displayManager->display();
            Serial.println("=== ACTUALIZACIÓN OTA COMPLETADA ===");
            delay(2000);
            ESP.restart();
        } else {
            _displayManager->setLedState(false);
            Serial.println("=== ERROR EN ACTUALIZACIÓN OTA ===");
        }
    });
}

void OTAUpdater::loop() {
    ElegantOTA.loop();
    
    // Actualizar efecto fade si está activo
    if (_systemStatus->isUpdating()) {
        _displayManager->updateFadeEffect();
    }
}
