#include "HomeAssistantMQTT.h"

// Singleton para callback estática de PubSubClient
HomeAssistantMQTT* HomeAssistantMQTT::_instance = nullptr;

// Intervalo mínimo entre reconexiones (backoff lineal: 5s, 10s, ... hasta 60s)
// Nunca se deja de reintentar: si el broker cae (reinicio de HA/Proxmox),
// el ESP32 debe reconectarse solo, sin tener que reiniciarlo.
static const unsigned long RECONNECT_INTERVAL = 5000;
static const unsigned long MAX_RECONNECT_INTERVAL = 60000;
static const uint8_t MAX_RECONNECT_ATTEMPTS = 10;  // A partir de aquí: reintento cada 60s

// ============================================================
// Constructor / Destructor
// ============================================================

HomeAssistantMQTT::HomeAssistantMQTT() : _mqttClient(_wifiClient) {
    _enabled = false;
    _connected = false;
    _discoveryPublished = false;
    _lastReport = 0;
    _lastReconnectAttempt = 0;
    _reconnectCount = 0;
    _commandCallback = nullptr;
    _instance = this;
}

HomeAssistantMQTT::~HomeAssistantMQTT() {
    disconnect();
    if (_instance == this) _instance = nullptr;
}

// ============================================================
// Configuración
// ============================================================

void HomeAssistantMQTT::setConfig(const Config& config) {
    _config = config;
    
    // Generar unique ID basado en MAC
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char macStr[13];
    snprintf(macStr, sizeof(macStr), "%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    _uniqueId = String("nivel_tinaco_") + macStr;
    
    // Topics MQTT
    _stateTopic   = "nivel_tinaco/" + String(macStr) + "/state";
    _commandTopic = "nivel_tinaco/" + String(macStr) + "/cmd";
    _availTopic   = "nivel_tinaco/" + String(macStr) + "/status";
}

bool HomeAssistantMQTT::isConfigured() const {
    return _config.brokerHost.length() > 0;
}

bool HomeAssistantMQTT::isConnected() const {
    return _connected;
}

String HomeAssistantMQTT::getStatusString() const {
    if (!_enabled) return "Deshabilitado";
    if (!isConfigured()) return "Sin configurar";
    if (_connected) return "Conectado";
    if (_reconnectCount >= MAX_RECONNECT_ATTEMPTS) return "Reintentando (cada 60s)";
    return "Desconectado";
}

// ============================================================
// Ciclo de vida
// ============================================================

bool HomeAssistantMQTT::begin() {
    if (!isConfigured()) {
        Serial.println("[HA-MQTT] ❌ No configurado (falta broker host)");
        return false;
    }
    if (!_enabled) {
        Serial.println("[HA-MQTT] ⏸️  Deshabilitado");
        return false;
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[HA-MQTT] ❌ WiFi no conectado");
        return false;
    }

    Serial.printf("[HA-MQTT] 🔌 Conectando a %s:%d...\n", 
                  _config.brokerHost.c_str(), _config.brokerPort);

    _mqttClient.setServer(_config.brokerHost.c_str(), _config.brokerPort);
    _mqttClient.setCallback(mqttCallbackStatic);
    _mqttClient.setBufferSize(1024);  // Discovery payloads pueden ser grandes
    _mqttClient.setKeepAlive(60);
    _mqttClient.setSocketTimeout(3);

    return reconnect();
}

void HomeAssistantMQTT::loop() {
    if (!_enabled || !isConfigured()) return;
    if (WiFi.status() != WL_CONNECTED) return;

    if (!_mqttClient.connected()) {
        _connected = false;
        // Reconexión con backoff (tope MAX_RECONNECT_INTERVAL, sin rendirse)
        unsigned long backoff = RECONNECT_INTERVAL * (_reconnectCount + 1);
        if (backoff > MAX_RECONNECT_INTERVAL) backoff = MAX_RECONNECT_INTERVAL;
        if (millis() - _lastReconnectAttempt > backoff) {
            if (reconnect()) {
                _reconnectCount = 0;
            } else if (_reconnectCount < 255) {
                _reconnectCount++;
            }
            _lastReconnectAttempt = millis();
        }
        return;
    }

    _mqttClient.loop();
}

void HomeAssistantMQTT::disconnect() {
    if (_mqttClient.connected()) {
        // Publicar offline antes de desconectar
        _mqttClient.publish(_availTopic.c_str(), "offline", true);
        _mqttClient.disconnect();
    }
    _connected = false;
    _discoveryPublished = false;
    Serial.println("[HA-MQTT] 🔌 Desconectado");
}

// ============================================================
// Reconexión MQTT
// ============================================================

bool HomeAssistantMQTT::reconnect() {
    if (_mqttClient.connected()) return true;
    if (WiFi.status() != WL_CONNECTED) return false;

    String clientId = buildClientId();
    
    Serial.printf("[HA-MQTT] 🔑 Conectando como: %s\n", clientId.c_str());

    // Last Will: si el ESP32 se desconecta, HA marca el dispositivo como offline
    bool connected = false;
    if (_config.username.length() > 0) {
        connected = _mqttClient.connect(
            clientId.c_str(),
            _config.username.c_str(),
            _config.password.c_str(),
            _availTopic.c_str(), 1, true, "offline"
        );
    } else {
        connected = _mqttClient.connect(
            clientId.c_str(),
            _availTopic.c_str(), 1, true, "offline"
        );
    }

    if (connected) {
        _connected = true;
        Serial.printf("[HA-MQTT] ✅ Conectado a %s:%d\n", 
                      _config.brokerHost.c_str(), _config.brokerPort);

        // Publicar disponibilidad
        _mqttClient.publish(_availTopic.c_str(), "online", true);

        // Suscribirse a topic de comandos
        _mqttClient.subscribe(_commandTopic.c_str());
        Serial.printf("[HA-MQTT] 📥 Suscrito a: %s\n", _commandTopic.c_str());

        // Publicar Auto-Discovery configs (solo una vez por conexión)
        if (!_discoveryPublished) {
            publishDiscoveryConfigs();
            _discoveryPublished = true;
        }
        
        // Publicar estado inicial de bomba (OFF al arrancar)
        String basePath = _stateTopic.substring(0, _stateTopic.lastIndexOf('/'));
        _mqttClient.publish((basePath + "/pump_state").c_str(), "OFF", true);

        return true;
    } else {
        Serial.printf("[HA-MQTT] ❌ Error de conexión: %d\n", _mqttClient.state());
        return false;
    }
}

String HomeAssistantMQTT::buildClientId() const {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char id[32];
    snprintf(id, sizeof(id), "esp32_nivel_%02x%02x%02x", mac[3], mac[4], mac[5]);
    return String(id);
}

// ============================================================
// Auto-Discovery de Home Assistant
// ============================================================

void HomeAssistantMQTT::publishDiscoveryConfigs() {
    Serial.println("[HA-MQTT] 📡 Publicando Auto-Discovery...");

    // Sensor: Litros de agua
    publishSingleDiscovery("sensor", "litros", 
        "Litros de Agua", "{{ value_json.litros }}", 
        "L", "volume_storage", "mdi:water");

    // Sensor: Porcentaje 
    publishSingleDiscovery("sensor", "porcentaje",
        "Nivel (%)", "{{ value_json.porcentaje }}",
        "%", nullptr, "mdi:water-percent");

    // Sensor: Distancia
    publishSingleDiscovery("sensor", "distancia",
        "Distancia", "{{ value_json.distancia_cm }}",
        "cm", "distance", "mdi:ruler");

    // Binary Sensor: Alerta de agua baja
    publishSingleDiscovery("binary_sensor", "alerta_baja",
        "Agua Baja", "{{ value_json.alerta_baja }}",
        nullptr, "problem", "mdi:alert-circle");

    // Binary Sensor: Llenando
    publishSingleDiscovery("binary_sensor", "llenando",
        "Llenando", "{{ value_json.llenando }}",
        nullptr, nullptr, "mdi:water-plus");

    // Binary Sensor: Sensor Inestable (detección de ecos falsos/multipath)
    publishSingleDiscovery("binary_sensor", "sensor_inestable",
        "Sensor Inestable", 
        "{{ 'true' if value_json.sensor_estable == 'false' else 'false' }}",
        nullptr, "problem", "mdi:sine-wave");

    // Binary Sensor: Modo Instalación
    publishSingleDiscovery("binary_sensor", "modo_instalacion",
        "Modo Instalacion", "{{ value_json.modo_instalacion }}",
        nullptr, nullptr, "mdi:wrench");

    // Sensor: WiFi Signal
    publishSingleDiscovery("sensor", "wifi_signal",
        "Señal WiFi", "{{ value_json.rssi }}",
        "dBm", "signal_strength", "mdi:wifi");

    // Switch: Bomba de agua (permite que HA active intervalo rápido)
    publishPumpSwitchDiscovery();

    Serial.println("[HA-MQTT] ✅ Auto-Discovery publicado (8 entidades + switch bomba)");
}

void HomeAssistantMQTT::publishPumpSwitchDiscovery() {
    // Publica un switch de HA que permite activar/desactivar el modo llenado
    // Cuando HA enciende este switch, el ESP32 pasa a intervalo rápido de sensado
    String topic = "homeassistant/switch/" + _uniqueId + "_bomba/config";
    
    // Extraer MAC del _stateTopic: "nivel_tinaco/{mac}/state" → "{mac}"
    String basePath = _stateTopic.substring(0, _stateTopic.lastIndexOf('/'));
    String pumpStateTopic = basePath + "/pump_state";
    
    StaticJsonDocument<768> doc;
    doc["name"] = "Bomba de Agua";
    doc["unique_id"] = _uniqueId + "_bomba";
    doc["command_topic"] = _commandTopic;  // nivel_tinaco/{mac}/cmd
    doc["state_topic"] = pumpStateTopic;   // nivel_tinaco/{mac}/pump_state
    doc["availability_topic"] = _availTopic;
    doc["payload_on"] = "FILLING_ON";
    doc["payload_off"] = "FILLING_OFF";
    doc["state_on"] = "ON";
    doc["state_off"] = "OFF";
    doc["icon"] = "mdi:water-pump";
    doc["retain"] = true;
    
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(_uniqueId);
    device["name"] = _config.deviceName;
    device["manufacturer"] = "ESP32 DIY";
    device["model"] = "Heltec WiFi Kit 32";
    device["sw_version"] = "3.3.3";
    
    String payload;
    serializeJson(doc, payload);
    _mqttClient.publish(topic.c_str(), payload.c_str(), true);
    Serial.println("[HA-MQTT] 💧 Switch 'Bomba de Agua' publicado en discovery");
}

void HomeAssistantMQTT::publishSingleDiscovery(
    const char* component, const char* objectId,
    const char* name, const char* valueTemplate,
    const char* unit, const char* deviceClass,
    const char* icon) 
{
    // Topic: homeassistant/{component}/{uniqueId}_{objectId}/config
    String topic = "homeassistant/" + String(component) + "/" + 
                   _uniqueId + "_" + objectId + "/config";

    StaticJsonDocument<768> doc;
    
    doc["name"] = name;
    doc["unique_id"] = _uniqueId + "_" + objectId;
    doc["state_topic"] = _stateTopic;
    doc["availability_topic"] = _availTopic;
    doc["payload_available"] = "online";
    doc["payload_not_available"] = "offline";
    doc["value_template"] = valueTemplate;
    
    if (unit) doc["unit_of_measurement"] = unit;
    if (deviceClass) doc["device_class"] = deviceClass;
    if (icon) doc["icon"] = icon;

    // Para binary_sensor: payload on/off
    if (String(component) == "binary_sensor") {
        doc["payload_on"] = "true";
        doc["payload_off"] = "false";
    }

    // Información del dispositivo (agrupa todas las entidades en un solo device en HA)
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(_uniqueId);
    device["name"] = _config.deviceName;
    device["manufacturer"] = "ESP32 DIY";
    device["model"] = "Heltec WiFi Kit 32";
    device["sw_version"] = "3.3.3";
    device["configuration_url"] = "http://" + WiFi.localIP().toString();

    String payload;
    serializeJson(doc, payload);

    bool published = _mqttClient.publish(topic.c_str(), payload.c_str(), true);
    if (!published) {
        Serial.printf("[HA-MQTT] ⚠️ Error publicando discovery: %s\n", objectId);
    }
}

// ============================================================
// Publicar datos del sensor
// ============================================================

void HomeAssistantMQTT::publishSensorData(float litros, float porcentaje, 
                                            float distancia, bool alertaBaja, 
                                            bool llenando, bool sensorEstable,
                                            bool modoInstalacion) 
{
    if (!isConnected()) return;

    StaticJsonDocument<512> doc;
    doc["litros"] = serialized(String(litros, 1));
    doc["porcentaje"] = serialized(String(porcentaje, 1));
    doc["distancia_cm"] = serialized(String(distancia, 1));
    doc["alerta_baja"] = alertaBaja ? "true" : "false";
    doc["llenando"] = llenando ? "true" : "false";
    doc["sensor_estable"] = sensorEstable ? "true" : "false";
    doc["modo_instalacion"] = modoInstalacion ? "true" : "false";
    doc["rssi"] = WiFi.RSSI();
    doc["uptime"] = millis() / 1000;

    String payload;
    serializeJson(doc, payload);

    bool published = _mqttClient.publish(_stateTopic.c_str(), payload.c_str());
    
    if (published) {
        _lastReport = millis();
        // Log reducido: solo cada 30s
        static unsigned long lastLog = 0;
        if (millis() - lastLog > 30000) {
            Serial.printf("[HA-MQTT] 📊 Datos: %.1fL (%.0f%%) dist=%.1fcm rssi=%d\n",
                         litros, porcentaje, distancia, WiFi.RSSI());
            lastLog = millis();
        }
    } else {
        Serial.println("[HA-MQTT] ⚠️ Error publicando datos");
    }
}

// ============================================================
// Callback MQTT
// ============================================================

void HomeAssistantMQTT::mqttCallbackStatic(char* topic, byte* payload, unsigned int length) {
    if (_instance) {
        _instance->handleMessage(topic, payload, length);
    }
}

void HomeAssistantMQTT::handleMessage(char* topic, byte* payload, unsigned int length) {
    // Convertir payload a String
    String message;
    message.reserve(length);
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    Serial.printf("[HA-MQTT] 📨 Mensaje en %s: %s\n", topic, message.c_str());

    // Pasar al callback del usuario
    if (_commandCallback) {
        _commandCallback(String(topic), message);
    }
}

bool HomeAssistantMQTT::publish(const char* topic, const char* payload, bool retained) {
    if (!isConnected()) return false;
    return _mqttClient.publish(topic, payload, retained);
}

bool HomeAssistantMQTT::publishPumpState(bool on) {
    if (!isConnected()) return false;
    String basePath = _stateTopic.substring(0, _stateTopic.lastIndexOf('/'));
    String pumpTopic = basePath + "/pump_state";
    return _mqttClient.publish(pumpTopic.c_str(), on ? "ON" : "OFF", true);
}
