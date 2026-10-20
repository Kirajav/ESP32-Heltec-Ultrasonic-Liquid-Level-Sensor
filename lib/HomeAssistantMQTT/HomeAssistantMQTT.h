#ifndef HOME_ASSISTANT_MQTT_H
#define HOME_ASSISTANT_MQTT_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

/**
 * @brief Integración MQTT local con Home Assistant Auto-Discovery.
 * 
 * Conecta el ESP32 al broker Mosquitto local (LAN) y publica
 * datos del sensor de nivel de agua. HA descubre automáticamente
 * el dispositivo vía MQTT Discovery.
 * 
 * Ventajas sobre Tuya Cloud:
 * - Sin TLS: conexión local, rápida, sin WiFiClientSecure (~10KB RAM menos)
 * - Sin dependencia de cloud externo
 * - Auto-Discovery: aparece automáticamente en Home Assistant
 * - Latencia mínima (LAN)
 * 
 * Requisitos:
 * 1. Home Assistant corriendo (ej. en Proxmox)
 * 2. Mosquitto MQTT broker instalado (addon de HA o standalone)
 * 3. MQTT Integration habilitada en HA
 */
class HomeAssistantMQTT {
public:
    struct Config {
        String brokerHost;          // IP del broker Mosquitto (ej: "192.168.1.100")
        uint16_t brokerPort = 1883; // Puerto MQTT (1883 default)
        String username;            // Usuario MQTT (puede estar vacío)
        String password;            // Contraseña MQTT (puede estar vacía)
        String deviceName = "Sensor Nivel Agua";  // Nombre visible en HA
        uint32_t reportInterval = 30000;  // ms entre reportes (30s default)
    };

    // Callback para comandos recibidos desde HA
    using CommandCallback = std::function<void(const String& topic, const String& payload)>;

    HomeAssistantMQTT();
    ~HomeAssistantMQTT();

    // Configuración
    void setConfig(const Config& config);
    void setCommandCallback(CommandCallback cb) { _commandCallback = cb; }

    // Ciclo de vida
    bool begin();       // Conectar al broker MQTT
    void loop();        // Llamar periódicamente
    void disconnect();  // Desconectar

    // Publicar datos del sensor
    void publishSensorData(float litros, float porcentaje, float distancia, 
                           bool alertaBaja, bool llenando,
                           bool sensorEstable = true,
                           bool modoInstalacion = false);

    // Estado
    bool isConnected() const;
    bool isConfigured() const;
    bool isEnabled() const { return _enabled; }
    void setEnabled(bool en) { _enabled = en; }
    String getStatusString() const;
    unsigned long getLastReportTime() const { return _lastReport; }
    String getUniqueId() const { return _uniqueId; }
    
    // Publicar mensaje genérico en un topic
    bool publish(const char* topic, const char* payload, bool retained = false);
    
    // Publicar estado de la bomba (ON/OFF) al topic correcto
    bool publishPumpState(bool on);

private:
    Config _config;
    WiFiClient _wifiClient;
    PubSubClient _mqttClient;
    CommandCallback _commandCallback;

    bool _enabled;
    bool _connected;
    bool _discoveryPublished;
    unsigned long _lastReport;
    unsigned long _lastReconnectAttempt;
    uint8_t _reconnectCount;
    String _uniqueId;       // MAC-based unique ID
    String _stateTopic;     // Topic para publicar estado
    String _commandTopic;   // Topic para recibir comandos
    String _availTopic;     // Topic de disponibilidad (online/offline)

    // Auto-Discovery
    void publishDiscoveryConfigs();
    void publishPumpSwitchDiscovery();
    void publishSingleDiscovery(const char* component, const char* objectId,
                                 const char* name, const char* valueTemplate,
                                 const char* unit, const char* deviceClass,
                                 const char* icon);

    // Conexión
    bool reconnect();
    String buildClientId() const;

    // Callback MQTT
    void handleMessage(char* topic, byte* payload, unsigned int length);
    static void mqttCallbackStatic(char* topic, byte* payload, unsigned int length);
    static HomeAssistantMQTT* _instance;
};

#endif // HOME_ASSISTANT_MQTT_H
