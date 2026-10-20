#ifndef MQTT_CONFIG_H
#define MQTT_CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * MQTT Broker Configuration
 * Almacena datos de conexión al broker Mosquitto local
 * para integración con Home Assistant
 */
class MQTTConfig {
private:
    String brokerHost;     // IP o hostname del broker (ej: "192.168.1.100")
    uint16_t brokerPort;   // Puerto MQTT (default: 1883)
    String username;       // Usuario MQTT (opcional)
    String password;       // Contraseña MQTT (opcional)
    String deviceName;     // Nombre del dispositivo en HA
    bool enabled;          // Habilitado/deshabilitado

public:
    MQTTConfig();

    // Getters
    String getBrokerHost() const { return brokerHost; }
    uint16_t getBrokerPort() const { return brokerPort; }
    String getUsername() const { return username; }
    String getPassword() const { return password; }
    String getDeviceName() const { return deviceName; }
    bool isEnabled() const { return enabled; }

    // Setters
    void setBrokerHost(const String& host) { brokerHost = host; }
    void setBrokerPort(uint16_t port) { brokerPort = port; }
    void setUsername(const String& user) { username = user; }
    void setPassword(const String& pass) { password = pass; }
    void setDeviceName(const String& name) { deviceName = name; }
    void setEnabled(bool e) { enabled = e; }

    // Validation
    bool isConfigured() const;
    bool isValid() const;

    // JSON serialization
    void toJson(JsonObject& json) const;
    void fromJson(const JsonObject& json);
};

#endif // MQTT_CONFIG_H
