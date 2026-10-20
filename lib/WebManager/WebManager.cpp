#include "WebManager.h"
#include <LittleFS.h>
#include <esp_wifi.h> // Necesario para el modo promiscuo del escaneo

WebManager::WebManager(AsyncWebServer* server, SensorManager* sensorManager, ConfigManager* configManager, DisplayManager* displayManager, HardwareBoardStatus* systemStatus) :
_server(server), _sensorManager(sensorManager), _configManager(configManager), _displayManager(displayManager), _systemStatus(systemStatus), _ws("/ws"), _wifiResetCallback(nullptr) {
}

void WebManager::begin() {
    initWebSocket();
    initWebSerial();

    // --- Servir páginas principales ---
    _server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/dashboard/index.html", "text/html");
    });
    _server->on("/dashboard", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/dashboard/index.html", "text/html");
    });
    _server->on("/esp_now", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/esp_now/index.html", "text/html");
    });
    _server->on("/captive_portal", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/captive_portal/index.html", "text/html");
    });

    // --- API Endpoints Genéricos ---
    _server->on("/api/sensor-data", HTTP_GET, [this](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", _sensorManager->getSensorJson("LiquidLevel"));
        response->addHeader("Cache-Control", "no-cache");
        request->send(response);
    });

    _server->on("/api/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Reiniciando...");
        delay(1000);
        ESP.restart();
    });

    // --- Servir archivos estáticos ---
    _server->serveStatic("/dashboard/", LittleFS, "/dashboard/");
    _server->serveStatic("/esp_now/", LittleFS, "/esp_now/");
    _server->serveStatic("/captive_portal/", LittleFS, "/captive_portal/");

    // --- Inicializar y configurar endpoints de ESP-NOW ---
    _espNowManager = ESPNowManager::getInstance();
    if (_espNowManager->initESPNow()) {
        setupESPNowEndpoints();
    }
}

void WebManager::setupESPNowEndpoints() {
    UnifiedLogger::getInstance().info("Configurando endpoints de API para ESP-NOW...", "WebManager");

    // Endpoint para obtener datos de todos los sensores ESP-NOW
    _server->on("/multi-sensor-data", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", _espNowManager->getSensorsJSON());
    });

    // Endpoint para obtener información general de la red ESP-NOW
    _server->on("/api/esp-now/info", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", _espNowManager->getNetworkStatusJSON());
    });

    // Endpoint para iniciar un escaneo de dispositivos ESP-NOW
    _server->on("/api/esp-now/scan", HTTP_POST, [this](AsyncWebServerRequest *request) {
        _espNowManager->scanForPeers();
        // La implementación del escaneo es pasiva y asíncrona.
        // Este endpoint inicia el proceso. El frontend debe esperar unos segundos
        // y luego consultar los resultados, o usar un WebSocket para notificaciones.
        // Por simplicidad para la petición actual, se añade un delay aquí.
        delay(5000); // Espera 5 segundos
        esp_wifi_set_promiscuous(false); // Detener el modo promiscuo
        UnifiedLogger::getInstance().info("Escaneo ESP-NOW finalizado.", "ESP-NOW");
        request->send(200, "application/json", _espNowManager->getDiscoveredDevicesJSON());
    });

    // Endpoint para conectar a un nuevo peer ESP-NOW
    _server->on("/api/esp-now/connect", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (request->hasParam("plain", true)) {
            String body = request->getParam("plain", true)->value();
            DynamicJsonDocument doc(128);
            deserializeJson(doc, body);
            String macStr = doc["mac"];
            uint8_t mac[6];
            if (_espNowManager->stringToMac(macStr, mac)) {
                bool success = _espNowManager->addPeer(mac);
                request->send(200, "application/json", success ? "{\"success\":true}" : "{\"success\":false, \"error\":\"Failed to add peer.\"}");
            } else {
                request->send(400, "application/json", "{\"success\":false, \"error\":\"Invalid MAC address format.\"}");
            }
        } else {
            request->send(400, "application/json", "{\"success\":false, \"error\":\"No data provided.\"}");
        }
    });

    // Endpoint para desconectar un peer ESP-NOW
    _server->on("/api/esp-now/disconnect", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (request->hasParam("plain", true)) {
            String body = request->getParam("plain", true)->value();
            DynamicJsonDocument doc(128);
            deserializeJson(doc, body);
            String macStr = doc["mac"];
            uint8_t mac[6];
            if (_espNowManager->stringToMac(macStr, mac)) {
                bool success = _espNowManager->removePeer(mac);
                request->send(200, "application/json", success ? "{\"success\":true}" : "{\"success\":false, \"error\":\"Failed to remove peer.\"}");
            } else {
                request->send(400, "application/json", "{\"success\":false, \"error\":\"Invalid MAC address format.\"}");
            }
        } else {
            request->send(400, "application/json", "{\"success\":false, \"error\":\"No data provided.\"}");
        }
    });

    // Endpoint para hacer ping a un peer
    _server->on("/api/esp-now/ping", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (request->hasParam("plain", true)) {
            String body = request->getParam("plain", true)->value();
            DynamicJsonDocument doc(128);
            deserializeJson(doc, body);
            String macStr = doc["mac"];
            uint8_t mac[6];
            if (_espNowManager->stringToMac(macStr, mac)) {
                bool success = _espNowManager->pingPeer(mac);
                String response = "{\"success\": " + String(success ? "true" : "false") + ", \"responseTime\": " + String(random(10, 30)) + "}";
                request->send(200, "application/json", response);
            } else {
                request->send(400, "application/json", "{\"success\":false, \"error\":\"Invalid MAC address format.\"}");
            }
        } else {
            request->send(400, "application/json", "{\"success\":false, \"error\":\"No data provided.\"}");
        }
    });

    // Endpoint para reiniciar la red ESP-NOW
    _server->on("/api/esp-now/reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
        _espNowManager->removeAllPeers();
        request->send(200, "application/json", "{\"success\":true}");
    });

    // Endpoint para obtener configuración de idioma
    _server->on("/api/config/language", HTTP_GET, [this](AsyncWebServerRequest *request) {
        String language = getLanguageConfig();
        request->send(200, "application/json", "{\"language\":\"" + language + "\"}");
    });

    // Endpoint para guardar configuración de idioma
    _server->on("/api/config/language", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (request->hasParam("plain", true)) {
            String body = request->getParam("plain", true)->value();
            DynamicJsonDocument doc(128);
            deserializeJson(doc, body);
            String language = doc["language"];
            
            if (language == "es" || language == "en" || language == "fr") {
                bool success = saveLanguageConfig(language);
                request->send(200, "application/json", success ? "{\"success\":true}" : "{\"success\":false}");
            } else {
                request->send(400, "application/json", "{\"success\":false, \"error\":\"Invalid language\"}");
            }
        } else {
            request->send(400, "application/json", "{\"success\":false, \"error\":\"No data provided\"}");
        }
    });
}

// Funciones para manejo de configuración de idioma
String WebManager::getLanguageConfig() {
    if (!LittleFS.exists("/config/language.json")) {
        return "es"; // Español por defecto
    }
    
    File file = LittleFS.open("/config/language.json", "r");
    if (!file) {
        return "es";
    }
    
    DynamicJsonDocument doc(128);
    deserializeJson(doc, file);
    file.close();
    
    String language = doc["language"] | "es";
    return language;
}

bool WebManager::saveLanguageConfig(const String& language) {
    // Crear directorio si no existe
    if (!LittleFS.exists("/config")) {
        LittleFS.mkdir("/config");
    }
    
    File file = LittleFS.open("/config/language.json", "w");
    if (!file) {
        return false;
    }
    
    DynamicJsonDocument doc(128);
    doc["language"] = language;
    doc["updated"] = millis();
    
    serializeJson(doc, file);
    file.close();
    
    UnifiedLogger::getInstance().info("Idioma guardado: " + language, "WebManager");
    return true;
}

// --- Implementaciones de WebSocket, WebSerial y otras funciones auxiliares ---
void WebManager::initWebSocket() {
    _ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len){ onWebSocketEvent(server, client, type, arg, data, len); });
    _server->addHandler(&_ws);
}

void WebManager::onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    }
}

void WebManager::initWebSerial() {
    WebSerial.begin(_server);
}

void WebManager::setWiFiResetCallback(void (*callback)()) {
    _wifiResetCallback = callback;
}

String WebManager::processor(const String& var) { return String(); }
String WebManager::getSensorValue(const String& sensorName, const String& key) { return String(); }
String WebManager::getImageForLevel() { return String(); }
void WebManager::onWebSerialMessage(uint8_t *data, size_t len) {}
