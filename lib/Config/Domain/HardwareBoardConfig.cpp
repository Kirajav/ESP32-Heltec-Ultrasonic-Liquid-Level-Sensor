#include "HardwareBoardConfig.h"

HardwareBoardConfig::HardwareBoardConfig() {
    setDefaults();
}

bool HardwareBoardConfig::loadFromJson(const JsonObject& json) {
    if (json.containsKey("display_on") || json.containsKey("display_enabled")) {
        // Support both old and new key names for backward compatibility
        settings.display_enabled = json.containsKey("display_enabled") ? 
            json["display_enabled"].as<bool>() : json["display_on"].as<bool>();
    }
    
    if (json.containsKey("autoSleepTime")) {
        settings.autoSleepTime = json["autoSleepTime"].as<uint16_t>();
    }
    
    if (json.containsKey("update_interval")) {
        settings.update_interval = json["update_interval"].as<uint16_t>();
    }
    
    if (json.containsKey("debug_enabled")) {
        settings.debug_enabled = json["debug_enabled"].as<bool>();
    }
    
    return isValid();
}

void HardwareBoardConfig::saveToJson(JsonObject& json) const {
    // Use new English key names
    json["display_enabled"] = settings.display_enabled;
    json["autoSleepTime"] = settings.autoSleepTime;
    json["update_interval"] = settings.update_interval;
    json["debug_enabled"] = settings.debug_enabled;
    
    // Keep old Spanish key for backward compatibility
    json["display_on"] = settings.display_enabled;
}

void HardwareBoardConfig::setDefaults() {
    settings.display_enabled = DEFAULT_DISPLAY_ENABLED;
    settings.autoSleepTime = DEFAULT_AUTO_SLEEP_TIME;
    settings.update_interval = DEFAULT_UPDATE_INTERVAL;
    settings.debug_enabled = DEFAULT_DEBUG_ENABLED;
}

void HardwareBoardConfig::setDisplayEnabled(bool enabled) {
    settings.display_enabled = enabled;
}

void HardwareBoardConfig::setAutoSleepTime(uint16_t seconds) {
    if (seconds >= 3 && seconds <= 300) { // 3 segundos a 5 minutos
        settings.autoSleepTime = seconds;
    }
}

void HardwareBoardConfig::setUpdateInterval(uint16_t interval) {
    if (interval >= 1 && interval <= 3600) { // 1 second to 1 hour
        settings.update_interval = interval;
    }
}

void HardwareBoardConfig::setDebugEnabled(bool enabled) {
    settings.debug_enabled = enabled;
}

bool HardwareBoardConfig::isValid() const {
    return settings.autoSleepTime >= 3 &&
           settings.autoSleepTime <= 300 &&
           settings.update_interval >= 1 &&
           settings.update_interval <= 3600;
}