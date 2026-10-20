#include "ApplicationConfig.h"

const char* ApplicationConfig::CONFIG_FILE = "/config.json";

ApplicationConfig::ApplicationConfig() {
    // Domain objects are initialized with their defaults
}

bool ApplicationConfig::begin() {
    if (!mountFilesystem()) {
        Serial.println("❌ FALLO: No se pudo montar LittleFS");
        return false;
    }
    Serial.println("✅ LittleFS montado correctamente");
    
    // Try to load existing configuration
    if (!loadFromFile()) {
        Serial.println("⚠️ No se pudo cargar config existente, guardando defaults...");
        // If loading fails, use defaults and save them
        saveToFile();
    } else {
        Serial.println("✅ Configuración cargada desde /config.json");
    }
    
    return true;
}

bool ApplicationConfig::mountFilesystem() {
    if (!LittleFS.begin(true)) {
        Serial.println("Error: Failed to mount LittleFS");
        return false;
    }
    return true;
}

bool ApplicationConfig::loadFromFile() {
    if (!LittleFS.exists(CONFIG_FILE)) {
        Serial.println("Config file doesn't exist, using defaults");
        return false;
    }
    
    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file) {
        Serial.println("Error: Failed to open config file for reading");
        return false;
    }
    
    DynamicJsonDocument doc(JSON_BUFFER_SIZE);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Serial.printf("Error: Failed to parse config JSON: %s\n", error.c_str());
        return false;
    }
    
    JsonObject root = doc.as<JsonObject>();
    
    // Load each domain configuration
    bool network_ok = true, sensor_ok = true, system_ok = true;
    
    if (root.containsKey("network")) {
        network_ok = network_config.loadFromJson(root["network"]);
    } else {
        // Backward compatibility: load from root level
        network_ok = network_config.loadFromJson(root);
    }
    
    if (root.containsKey("sensor")) {
        sensor_ok = sensor_config.loadFromJson(root["sensor"]);
    } else {
        // Backward compatibility: load from root level
        sensor_ok = sensor_config.loadFromJson(root);
    }
    
    if (root.containsKey("system")) {
        system_ok = system_config.loadFromJson(root["system"]);
    } else {
        // Backward compatibility: load from root level
        system_ok = system_config.loadFromJson(root);
    }
    
    // Load ESP-NOW configuration
    bool espnow_ok = true;
    if (root.containsKey("espnow")) {
        espnow_config.fromJson(root["espnow"]);
    } else {
        // Initialize with defaults if not present
        espnow_config.initialize();
    }
    
    // Load MQTT configuration (Home Assistant)
    if (root.containsKey("mqtt")) {
        mqtt_config.fromJson(root["mqtt"]);
    }
    // Backward compatibility: migrate old Tuya config
    if (root.containsKey("tuya") && !root.containsKey("mqtt")) {
        // Old Tuya config exists but no MQTT config — leave MQTT disabled
        Serial.println("⚠️ Config Tuya antigua encontrada, MQTT deshabilitado por defecto");
    }
    
    if (!network_ok || !sensor_ok || !system_ok || !espnow_ok) {
        Serial.println("Warning: Some configuration domains failed validation, using defaults");
    }
    
    // Diagnostic: show loaded WiFi networks
    auto networks = network_config.getWiFiNetworks();
    Serial.printf("📋 Config cargada: %d redes WiFi\n", networks.size());
    for (const auto& net : networks) {
        Serial.printf("   📡 SSID: '%s' default=%s\n", net.ssid.c_str(), net.is_default ? "SÍ" : "no");
    }
    
    return true;
}

bool ApplicationConfig::saveToFile() {
    DynamicJsonDocument doc(JSON_BUFFER_SIZE);
    JsonObject root = doc.to<JsonObject>();
    
    // Save each domain to its own section
    JsonObject network_obj = root.createNestedObject("network");
    network_config.saveToJson(network_obj);
    
    JsonObject sensor_obj = root.createNestedObject("sensor");
    sensor_config.saveToJson(sensor_obj);
    
    JsonObject system_obj = root.createNestedObject("system");
    system_config.saveToJson(system_obj);
    
    JsonObject espnow_obj = root.createNestedObject("espnow");
    espnow_config.toJson(espnow_obj);
    
    JsonObject mqtt_obj = root.createNestedObject("mqtt");
    mqtt_config.toJson(mqtt_obj);
    
    // Also save to root level for backward compatibility
    network_config.saveToJson(root);
    sensor_config.saveToJson(root);
    system_config.saveToJson(root);
    
    // Diagnostic: check buffer usage
    size_t memUsed = doc.memoryUsage();
    Serial.printf("💾 JSON buffer: %d/%d bytes usado (%d%%)\n", memUsed, JSON_BUFFER_SIZE, (memUsed * 100) / JSON_BUFFER_SIZE);
    if (memUsed > JSON_BUFFER_SIZE * 90 / 100) {
        Serial.println("⚠️ ADVERTENCIA: Buffer JSON casi lleno, posible pérdida de datos");
    }
    
    // Diagnostic: verify wifi_networks in JSON before writing
    if (root.containsKey("network") && root["network"].containsKey("wifi_networks")) {
        Serial.printf("💾 WiFi networks en JSON a guardar: %d\n", root["network"]["wifi_networks"].size());
    } else {
        Serial.println("⚠️ wifi_networks NO encontrado en JSON a guardar!");
    }
    
    File file = LittleFS.open(CONFIG_FILE, "w");
    if (!file) {
        Serial.println("Error: Failed to open config file for writing");
        return false;
    }
    
    size_t bytes_written = serializeJson(doc, file);
    file.close();
    
    if (bytes_written == 0) {
        Serial.println("Error: Failed to write config file");
        return false;
    }
    
    Serial.printf("✅ Configuration saved (%d bytes written to %s)\n", bytes_written, CONFIG_FILE);
    return true;
}

bool ApplicationConfig::save() {
    return saveToFile();
}

bool ApplicationConfig::load() {
    return loadFromFile();
}

void ApplicationConfig::reset() {
    network_config.setDefaults();
    sensor_config.setDefaults();
    system_config.setDefaults();
    saveToFile();
    Serial.println("Configuration reset to defaults");
}

bool ApplicationConfig::isValid() const {
    return network_config.isValid() && 
           sensor_config.isValid() && 
           system_config.isValid();
}

bool ApplicationConfig::exportToJson(String& json_string) const {
    DynamicJsonDocument doc(JSON_BUFFER_SIZE);
    JsonObject root = doc.to<JsonObject>();
    
    // Export each domain
    JsonObject network_obj = root.createNestedObject("network");
    network_config.saveToJson(network_obj);
    
    JsonObject sensor_obj = root.createNestedObject("sensor");
    sensor_config.saveToJson(sensor_obj);
    
    JsonObject system_obj = root.createNestedObject("system");
    system_config.saveToJson(system_obj);
    
    JsonObject mqtt_obj = root.createNestedObject("mqtt");
    mqtt_config.toJson(mqtt_obj);
    
    // Also export to root level for compatibility
    network_config.saveToJson(root);
    sensor_config.saveToJson(root);
    system_config.saveToJson(root);
    
    json_string = "";
    serializeJsonPretty(doc, json_string);
    
    return !json_string.isEmpty();
}

bool ApplicationConfig::importFromJson(const String& json_string) {
    DynamicJsonDocument doc(JSON_BUFFER_SIZE);
    DeserializationError error = deserializeJson(doc, json_string);
    
    if (error) {
        Serial.printf("Error: Failed to parse import JSON: %s\n", error.c_str());
        return false;
    }
    
    JsonObject root = doc.as<JsonObject>();
    
    // Load each domain
    bool success = true;
    
    if (root.containsKey("network")) {
        success &= network_config.loadFromJson(root["network"]);
    } else {
        success &= network_config.loadFromJson(root);
    }
    
    if (root.containsKey("sensor")) {
        success &= sensor_config.loadFromJson(root["sensor"]);
    } else {
        success &= sensor_config.loadFromJson(root);
    }
    
    if (root.containsKey("system")) {
        success &= system_config.loadFromJson(root["system"]);
    } else {
        success &= system_config.loadFromJson(root);
    }
    
    // Load MQTT configuration if present
    if (root.containsKey("mqtt")) {
        mqtt_config.fromJson(root["mqtt"]);
    }
    
    if (success) {
        saveToFile();
        Serial.println("Configuration imported successfully");
    } else {
        Serial.println("Error: Configuration import failed validation");
    }
    
    return success;
}
