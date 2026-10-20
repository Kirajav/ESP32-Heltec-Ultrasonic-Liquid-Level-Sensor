#ifndef HARDWARE_BOARD_CONFIG_H
#define HARDWARE_BOARD_CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * Hardware Board Configuration Domain
 * 
 * Handles Heltec WiFi Kit 32 hardware-related settings such as:
 * - Display configuration (enabled, auto-sleep)
 * - Update intervals
 * - Debug settings
 * - LED controls
 * - Board-specific features
 */

struct HardwareBoardSettings {
    bool display_enabled;
    uint16_t autoSleepTime;   // in seconds
    uint16_t update_interval;  // in seconds
    bool debug_enabled;
};

class HardwareBoardConfig {
public:
    HardwareBoardConfig();
    
    // JSON operations
    bool loadFromJson(const JsonObject& json);
    void saveToJson(JsonObject& json) const;
    void setDefaults();
    
    // Display settings
    void setDisplayEnabled(bool enabled);
    bool isDisplayEnabled() const { return settings.display_enabled; }
    
    void setAutoSleepTime(uint16_t seconds);
    uint16_t getAutoSleepTime() const { return settings.autoSleepTime; }
    
    // Update interval
    void setUpdateInterval(uint16_t interval);
    uint16_t getUpdateInterval() const { return settings.update_interval; }
    
    // Debug settings
    void setDebugEnabled(bool enabled);
    bool isDebugEnabled() const { return settings.debug_enabled; }
    
    // Access to settings
    const HardwareBoardSettings& getSettings() const { return settings; }
    
    // Validation
    bool isValid() const;
    
private:
    HardwareBoardSettings settings;
    
    // Default values
    static const bool DEFAULT_DISPLAY_ENABLED = true;
    static const uint16_t DEFAULT_AUTO_SLEEP_TIME = 30;  // 30 seconds
    static const uint16_t DEFAULT_UPDATE_INTERVAL = 10;  // 10 seconds
    static const bool DEFAULT_DEBUG_ENABLED = false;
};

#endif // HARDWARE_BOARD_CONFIG_H