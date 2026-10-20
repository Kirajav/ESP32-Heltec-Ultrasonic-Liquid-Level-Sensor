#ifndef APP_MANAGER_H
#define APP_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ESP_DoubleResetDetector.h>
#include <WebSerial.h>
#include <ElegantOTA.h>
#include <ESPmDNS.h>
#include "DisplayManager.h"
#include "SensorManager.h"
#include "../lib/Config/ApplicationConfig.h"
#include "ESPNowManager.h"
#include "HomeAssistantMQTT.h"
#include "NTPTimeSync.h"
#include "LiquidLevelSensor.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define FIRMWARE_VERSION "3.4.2"

class AppManager {
public:
    AppManager();
    void initialize();
    void loop();
    bool isWiFiConnected();
    String getDeviceIP();
    void restart();

private:
    DisplayManager display_manager;
    SensorManager sensor_manager;
    ApplicationConfig config_manager;
    bool wifi_connected;
    bool portal_active;
    bool forcePortalMode;  // Para forzar portal cautivo con doble reset
    
    // Auto-reboot: si el portal cautivo está activo sin clientes y hay WiFi guardada,
    // reiniciar tras 5 minutos para intentar reconectar (ej: tras corte de luz)
    static constexpr unsigned long AP_IDLE_REBOOT_MS = 5UL * 60UL * 1000UL; // 5 minutos
    unsigned long _portalStartTime;
    
    // Variables para chequeo de conexión WiFi
    unsigned long connectionCheckTimer;
    bool checkingConnection;

    // Ahorro de energía (calentamiento): CPU a 80 MHz y modem sleep WiFi
    static constexpr uint32_t CPU_FREQ_MHZ = 80;
    bool _wifiPowerSave = false;
    void updateWiFiPowerSave(bool force = false);

    // ESP-NOW no recibe con modem sleep: ventana de descubrimiento con la radio
    // despierta (al arrancar y al pulsar "Escanear") enviando PING cada 5s
    static constexpr unsigned long ESPNOW_DISCOVERY_MS = 180000; // 3 minutos
    unsigned long _espNowDiscoveryStart = 0;
    bool _espNowDiscovering = false;
    void openEspNowDiscovery();
    
    // Variables para auto-sleep de pantalla
    unsigned long lastDisplayActivity;
    bool displaySleeping;
    
    // IoT Integrations
    ESPNowManager* espNowManager;
    HomeAssistantMQTT* haMqtt;
    NTPTimeSync* ntpSync;
    LiquidLevelSensor* _liquidSensor = nullptr; // Referencia directa al sensor para diagnóstico
    
    // FreeRTOS task for HA MQTT (non-blocking)
    TaskHandle_t _mqttTaskHandle;
    volatile bool _mqttReconfigPending;     // Flag: POST endpoint requests reconfig
    volatile bool _mqttTaskRunning;         // Flag: task is alive
    HomeAssistantMQTT::Config _mqttPendingConfig;    // New config from POST endpoint
    volatile bool _mqttPendingEnabled;      // New enabled state from POST endpoint
    static void mqttTaskFunc(void* param);  // FreeRTOS task function
    
    void initializeNetwork();
    void startPortalMode();       // Iniciar portal cautivo
    void startNormalMode();       // Iniciar modo sensor normal
    bool tryConnectWiFi();        // Intentar conectar WiFi (default + fallback)
    bool tryConnectToNetwork(const WiFiNetwork& network); // Conectar a una red específica
    void setForceNormalBoot();    // Escribir flag para forzar modo normal tras reinicio
    bool checkForceNormalBoot();  // Leer y limpiar flag de modo normal forzado
    void setForcePortalBoot();    // Escribir flag para forzar portal cautivo tras reinicio
    bool checkForcePortalBoot();  // Leer y limpiar flag de portal forzado
    void setupWebServer();        // Web server unificado inteligente
    void setupStaticFiles();      // Archivos estáticos compartidos
    void setupSmartRoutes();      // Rutas principales inteligentes
    void setupConditionalRoutes(); // Rutas específicas por modo
    void setupCaptiveDetectionRoutes(); // Detección portal cautivo
    void initializeServices();    // Servicios según modo
    void handleScanWiFi(AsyncWebServerRequest *request);
    void handleWiFiResults(AsyncWebServerRequest *request);
    void handleSensorData(AsyncWebServerRequest *request);
    void handleToggleDisplay(AsyncWebServerRequest *request);
    void updateSensorDisplay(bool forceRedraw = false);       // Actualizar pantalla con datos del sensor
    void updatePortalDisplay();       // Actualizar pantalla del portal cautivo
    void updateHAData(bool isFilling); // Reportar datos a Home Assistant
    void handleHACommand(const String& topic, const String& payload); // Comandos de HA
    void apCallback();
    void wakeUpDisplay();      // Despertar pantalla por actividad
    void checkAutoSleep();     // Verificar si debe apagar pantalla
    
    // WebSerial live monitor
    volatile bool _liveMonitorEnabled = false;
    
    // Contador de lecturas reales del sensor (para dedup en dashboard)
    volatile uint32_t _sensorReadingId = 0;
    
    // Modo instalación: lecturas raw de distancia para posicionar el sensor
    volatile bool _installationMode = false;
    void updateInstallationDisplay(float distancia);
    static constexpr unsigned long INSTALL_READ_INTERVAL_MS = 700; // + ~0.3 s de muestreo = ~1 lectura/s
    static constexpr float INSTALL_WAITING = -2.0f; // _installRawCm antes de la primera lectura cruda
    volatile float _installRawCm = -1.0f;      // Última distancia cruda en modo instalación (-1 = sin eco)
    volatile bool _forceSensorRead = false;    // Lectura normal inmediata (al salir del modo)
    volatile bool _resetFlowHistory = false;   // Borrar historial de flujo (al salir del modo)
    void setInstallationMode(bool on);
    float calibrationDistance();
    
    // Modo llenado forzado (toggle via WebSerial)
    volatile bool _forceFillingMode = false;
    unsigned long _forceFillingTimestamp = 0;
    static const unsigned long FORCE_FILLING_TIMEOUT_MS = 120000; // 2 min auto-timeout
    
    // Flow state detection (filling/draining/idle)
    FlowState detectFlowState(float readings[], uint8_t threshold, uint8_t currentIndex);
    bool detectFilling(float readings[], uint8_t threshold, uint8_t currentIndex); // Legacy wrapper
    
    // Historial circular de lecturas del sensor (para log y futuras gráficas)
    static const uint8_t HISTORY_SIZE = 60; // Últimas 60 lecturas
    struct SensorHistoryEntry {
        uint32_t timestamp;  // Unix epoch (0 = no NTP)
        float litros;
        float distancia;
        float porcentaje;
        char estado[12];
    };
    SensorHistoryEntry _sensorHistory[60];
    uint8_t _historyIndex = 0;
    uint8_t _historyCount = 0;
    void addSensorHistoryEntry(float litros, float distancia, float porcentaje, const String& estado);
};

#endif
