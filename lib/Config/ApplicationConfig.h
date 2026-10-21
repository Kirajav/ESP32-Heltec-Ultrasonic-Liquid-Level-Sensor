#ifndef APPLICATION_CONFIG_H
#define APPLICATION_CONFIG_H

#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "Domain/NetworkConfig.h"
#include "Domain/SensorConfig.h"
#include "Domain/HardwareBoardConfig.h"
#include "Domain/ESPNowConfig.h"
#include "Domain/MQTTConfig.h"

/**
 * Application Configuration Manager - Domain-driven architecture
 * 
 * This class acts as a facade that coordinates specialized 
 * configuration domains:
 * - NetworkConfig: Network-related settings (WiFi, hostname, etc.)
 * - SensorConfig: Sensor-related settings (measurements, calibration)
 * - HardwareBoardConfig: Heltec board hardware settings (display, debugging, etc.)
 * - ESPNowConfig: ESP-NOW mesh network configuration
 * - MQTTConfig: MQTT broker configuration for Home Assistant
 * 
 * Benefits:
 * - Single Responsibility Principle: Each domain handles its own concerns
 * - Better testability and maintainability
 * - Clean separation of concerns
 * - Professional naming conventions
 */
class ApplicationConfig {
private:
    NetworkConfig network_config;
    SensorConfig sensor_config;
    HardwareBoardConfig system_config;
    ESPNowConfig espnow_config;
    MQTTConfig mqtt_config;
    
    static const char* CONFIG_FILE;
    static const size_t JSON_BUFFER_SIZE = 6144;  // 3.5.0: + clave y unidades ESP-NOW
    
    SemaphoreHandle_t _mutex = nullptr;
    
    bool mountFilesystem();
    bool loadFromFile();
    bool saveToFile();
    bool saveToFileLocked();
    
public:
    ApplicationConfig();
    
    // Initialization
    bool begin();
    
    // Guardados y cambios desde varias tareas (loop principal y servidor web): el loop
    // modifica la config ESP-NOW mientras un handler puede estar serializándola
    void lock() { if (_mutex) xSemaphoreTakeRecursive(_mutex, portMAX_DELAY); }
    void unlock() { if (_mutex) xSemaphoreGiveRecursive(_mutex); }
    
    // Global operations
    bool save();
    bool load();
    void reset();
    
    // Network configuration access
    NetworkConfig& getNetworkConfig() { return network_config; }
    const NetworkConfig& getNetworkConfig() const { return network_config; }
    
    // Sensor configuration access
    SensorConfig& getSensorConfig() { return sensor_config; }
    const SensorConfig& getSensorConfig() const { return sensor_config; }
    
    // System configuration access
    HardwareBoardConfig& getSystemConfig() { return system_config; }
    const HardwareBoardConfig& getSystemConfig() const { return system_config; }
    
    // ESP-NOW configuration access
    ESPNowConfig& getESPNowConfig() { return espnow_config; }
    const ESPNowConfig& getESPNowConfig() const { return espnow_config; }
    
    // MQTT configuration access (Home Assistant)
    MQTTConfig& getMQTTConfig() { return mqtt_config; }
    const MQTTConfig& getMQTTConfig() const { return mqtt_config; }
    
    // MQTT convenience methods
    String getMQTTBrokerHost() const { return mqtt_config.getBrokerHost(); }
    uint16_t getMQTTBrokerPort() const { return mqtt_config.getBrokerPort(); }
    String getMQTTUsername() const { return mqtt_config.getUsername(); }
    String getMQTTDeviceName() const { return mqtt_config.getDeviceName(); }
    void setMQTTBroker(const String& host, uint16_t port, const String& user, const String& pass) {
        mqtt_config.setBrokerHost(host);
        mqtt_config.setBrokerPort(port);
        mqtt_config.setUsername(user);
        mqtt_config.setPassword(pass);
        mqtt_config.setEnabled(true);
    }
    
    // Convenience methods for backward compatibility
    // Network
    String getHostname() const { return network_config.getSettings().hostname; }
    void setHostname(const String& hostname) { network_config.setHostname(hostname); }
    
    String getAPSSID() const { return network_config.getSettings().ap_ssid; }
    void setAPSSID(const String& ssid) { network_config.setApSSID(ssid); }
    
    String getAPPassword() const { return network_config.getSettings().ap_password; }
    void setAPPassword(const String& password) { network_config.setApPassword(password); }
    
    // Station WiFi configuration  
    String getWiFiSSID() const { return network_config.getSettings().wifi_ssid; }
    String getWiFiPassword() const { return network_config.getSettings().wifi_password; }
    bool setWiFiCredentials(const String& ssid, const String& password) { 
        bool credentialsValid = network_config.setWiFiCredentials(ssid, password);
        if (!credentialsValid) {
            return false; // Credenciales inválidas
        }
        return save(); // Guardar solo si las credenciales son válidas
    }
    
    bool getCheckUpdates() const { return network_config.getSettings().check_updates; }
    void setCheckUpdates(bool check) { network_config.setCheckUpdates(check); }
    
    // Sensor
    float getMaxHeight() const { return sensor_config.getSettings().max_height; }
    void setMaxHeight(float height) { sensor_config.setMaxHeight(height); }
    
    float getCapacity() const { return sensor_config.getSettings().capacity; }
    void setCapacity(float capacity) { sensor_config.setCapacity(capacity); }
    
    float getMinDistance() const { return sensor_config.getSettings().min_distance; }
    void setMinDistance(float distance) { sensor_config.setMinDistance(distance); }
    
    ContainerType getContainerType() const { return sensor_config.getSettings().container_type; }
    void setContainerType(ContainerType type) { sensor_config.setContainerType(type); }
    String getContainerTypeString() const { return sensor_config.getContainerTypeString(); }
    
    // System
    bool getDisplayEnabled() const { return system_config.getSettings().display_enabled; }
    void setDisplayEnabled(bool enabled) { system_config.setDisplayEnabled(enabled); }
    
    uint16_t getAutoSleepTime() const { return system_config.getSettings().autoSleepTime; }
    void setAutoSleepTime(uint16_t seconds) { system_config.setAutoSleepTime(seconds); }
    
    uint16_t getUpdateInterval() const { return system_config.getSettings().update_interval; }
    void setUpdateInterval(uint16_t interval) { system_config.setUpdateInterval(interval); }
    
    bool getDebugEnabled() const { return system_config.getSettings().debug_enabled; }
    void setDebugEnabled(bool enabled) { system_config.setDebugEnabled(enabled); }
    
    // Validation
    bool isValid() const;
    
    // JSON operations
    bool exportToJson(String& json_string) const;
    bool importFromJson(const String& json_string);
};

#endif // APPLICATION_CONFIG_H