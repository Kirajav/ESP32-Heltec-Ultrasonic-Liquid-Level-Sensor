#include "ESPNowConfig.h"
#include <WiFi.h>
#include <esp_system.h>

ESPNowConfig::ESPNowConfig() {
    // Default values
    isMasterMode = false;
    isSlaveMode = false;
    customSensorID = 0;
    customSensorName = "";
    useAutoID = true;
    useAutoName = true;
    
    // Default network settings
    transmissionPower = 100;  // 100% power
    sendInterval = 60000;     // 60 seconds
    enableSleep = true;       // Enable sleep by default
}

void ESPNowConfig::initialize() {
    generateAutoIdentity();
    
    // Respetar el rol guardado (/api/esp-now/configure); antes se sobrescribía
    // en cada arranque y todas las unidades con WiFi quedaban como master
    if (isMasterMode || isSlaveMode) return;

    // Auto-detect role based on hardware or connectivity
    // If no WiFi credentials stored, default to slave mode
    // If WiFi credentials exist, default to master mode
    if (WiFi.SSID().length() > 0) {
        setMasterMode(true);
    } else {
        setSlaveMode(true);
    }
}

void ESPNowConfig::generateAutoIdentity() {
    // Get device MAC address
    deviceMAC = WiFi.macAddress();
    
    // Get chip ID (unique per ESP32)
    uint32_t chipId = 0;
    for (int i = 0; i < 17; i = i + 8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    deviceChipID = String(chipId, HEX);
    deviceChipID.toUpperCase();
    
    // Generate auto sensor ID from chip ID (1-254 range)
    autoSensorID = (chipId % 254) + 1;
    
    // Generate auto sensor name
    autoSensorName = "Sensor-" + deviceChipID.substring(0, 6);
    
    Serial.printf("🆔 Auto-generated identity:\n");
    Serial.printf("   MAC: %s\n", deviceMAC.c_str());
    Serial.printf("   Chip ID: %s\n", deviceChipID.c_str());
    Serial.printf("   Auto Sensor ID: %d\n", autoSensorID);
    Serial.printf("   Auto Sensor Name: %s\n", autoSensorName.c_str());
}

void ESPNowConfig::setMasterMode(bool enabled) {
    isMasterMode = enabled;
    if (enabled) {
        isSlaveMode = false;
        Serial.println("🎛️ Configurado como MASTER ESP-NOW");
    }
}

void ESPNowConfig::setSlaveMode(bool enabled) {
    isSlaveMode = enabled;
    if (enabled) {
        isMasterMode = false;
        Serial.println("📡 Configurado como SLAVE ESP-NOW");
    }
}

uint8_t ESPNowConfig::getSensorID() const {
    if (useAutoID || customSensorID == 0) {
        return autoSensorID;
    }
    return customSensorID;
}

String ESPNowConfig::getSensorName() const {
    if (useAutoName || customSensorName.length() == 0) {
        return autoSensorName;
    }
    return customSensorName;
}

void ESPNowConfig::setCustomSensorID(uint8_t id) {
    if (id >= 1 && id <= 254) {
        customSensorID = id;
        useAutoID = false;
        Serial.printf("🆔 Sensor ID personalizado: %d\n", id);
    }
}

void ESPNowConfig::setCustomSensorName(const String& name) {
    if (name.length() > 0 && name.length() <= 31) {
        customSensorName = name;
        useAutoName = false;
        Serial.printf("🏷️ Nombre sensor personalizado: %s\n", name.c_str());
    }
}

void ESPNowConfig::toJson(JsonObject& json) const {
    // Device identity
    json["deviceMAC"] = deviceMAC;
    json["deviceChipID"] = deviceChipID;
    json["autoSensorID"] = autoSensorID;
    json["autoSensorName"] = autoSensorName;
    
    // Role configuration
    json["isMasterMode"] = isMasterMode;
    json["isSlaveMode"] = isSlaveMode;
    
    // Custom identity
    json["customSensorID"] = customSensorID;
    json["customSensorName"] = customSensorName;
    json["useAutoID"] = useAutoID;
    json["useAutoName"] = useAutoName;
    
    // Network settings
    json["transmissionPower"] = transmissionPower;
    json["sendInterval"] = sendInterval;
    json["enableSleep"] = enableSleep;
}

void ESPNowConfig::fromJson(const JsonObject& json) {
    // Role configuration
    isMasterMode = json["isMasterMode"] | false;
    isSlaveMode = json["isSlaveMode"] | false;
    
    // Custom identity
    customSensorID = json["customSensorID"] | 0;
    customSensorName = json["customSensorName"] | "";
    useAutoID = json["useAutoID"] | true;
    useAutoName = json["useAutoName"] | true;
    
    // Network settings
    transmissionPower = json["transmissionPower"] | 100;
    sendInterval = json["sendInterval"] | 60000;
    enableSleep = json["enableSleep"] | true;
    
    // Re-generate auto identity if needed
    if (deviceMAC.length() == 0) {
        generateAutoIdentity();
    }
}

String ESPNowConfig::toJsonString() const {
    DynamicJsonDocument doc(512);
    JsonObject json = doc.to<JsonObject>();
    toJson(json);
    
    String result;
    serializeJson(doc, result);
    return result;
}

bool ESPNowConfig::fromJsonString(const String& jsonStr) {
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    if (error) {
        Serial.printf("❌ Error parsing ESP-NOW config JSON: %s\n", error.c_str());
        return false;
    }
    
    fromJson(doc.as<JsonObject>());
    return true;
}

bool ESPNowConfig::isValid() const {
    // Must have either master or slave mode (but not both)
    if (isMasterMode && isSlaveMode) return false;
    if (!isMasterMode && !isSlaveMode) return false;
    
    // Sensor ID must be valid range
    uint8_t sensorId = getSensorID();
    if (sensorId < 1 || sensorId > 254) return false;
    
    // Sensor name must not be empty
    if (getSensorName().length() == 0) return false;
    
    // Send interval must be reasonable
    if (sendInterval < 1000 || sendInterval > 3600000) return false; // 1s to 1h
    
    return true;
}

String ESPNowConfig::getValidationError() const {
    if (isMasterMode && isSlaveMode) {
        return "No puede ser Master y Slave al mismo tiempo";
    }
    if (!isMasterMode && !isSlaveMode) {
        return "Debe seleccionar modo Master o Slave";
    }
    
    uint8_t sensorId = getSensorID();
    if (sensorId < 1 || sensorId > 254) {
        return "Sensor ID debe estar entre 1 y 254";
    }
    
    if (getSensorName().length() == 0) {
        return "Nombre del sensor no puede estar vacío";
    }
    
    if (sendInterval < 1000 || sendInterval > 3600000) {
        return "Intervalo debe estar entre 1 segundo y 1 hora";
    }
    
    return "";
}