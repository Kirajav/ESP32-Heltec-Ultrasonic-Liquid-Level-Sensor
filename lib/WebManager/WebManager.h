#ifndef WEB_MANAGER_H
#define WEB_MANAGER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>
#include <ArduinoJson.h>
#include "SensorManager.h"
#include "../Config/ApplicationConfig.h"
#include "DisplayManager.h"
#include "HardwareBoardStatus.h"
#include "ESPNowManager.h"
#include "GoogleHomeIntegration.h"
#include "AlexaIntegration.h"
#include "TuyaIntegration.h"

class WebManager {
public:
    WebManager(AsyncWebServer* server, SensorManager* sensorManager, ConfigManager* configManager, DisplayManager* displayManager, HardwareBoardStatus* systemStatus);
    void begin();
    void setWiFiResetCallback(void (*callback)());  // Nuevo callback para reset WiFi
    
    // ESP-NOW endpoints
    void setupESPNowEndpoints();
    void handleMultiSensorData();
    void handleESPNowStatus();

private:
    AsyncWebServer* _server;
    SensorManager* _sensorManager;
    ConfigManager* _configManager;
    DisplayManager* _displayManager;
    HardwareBoardStatus* _systemStatus;
    AsyncWebSocket _ws;
    void (*_wifiResetCallback)();  // Callback para reset WiFi
    ESPNowManager* _espNowManager;  // ESP-NOW manager
    GoogleHomeIntegration* _googleHome;  // Google Home integration
    AlexaIntegration* _alexa;  // Alexa integration
    TuyaIntegration* _tuya;  // Tuya Smart integration

    void initWebSocket();
    void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void initWebSerial();
    void onWebSerialMessage(uint8_t *data, size_t len);
    String processor(const String& var);
    String getSensorValue(const String& sensorName, const String& key);
    String getImageForLevel();
    
    // Funciones para manejo de idioma
    String getLanguageConfig();
    bool saveLanguageConfig(const String& language);
};

#endif // WEB_MANAGER_H
