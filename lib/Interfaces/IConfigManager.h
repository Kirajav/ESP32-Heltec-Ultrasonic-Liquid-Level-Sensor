#ifndef ICONFIG_MANAGER_H
#define ICONFIG_MANAGER_H

#include <Arduino.h>

/**
 * @brief Interface for configuration management
 * 
 * This interface defines the contract for configuration management,
 * enabling different storage backends and facilitating testing.
 */
class IConfigManager {
public:
    virtual ~IConfigManager() = default;
    
    // Core configuration operations
    virtual bool begin() = 0;
    virtual bool save() = 0;
    virtual bool load() = 0;
    virtual void reset() = 0;
    virtual bool isValid() const = 0;
    
    // Network configuration
    virtual String getHostname() const = 0;
    virtual void setHostname(const String& hostname) = 0;
    virtual String getAPSSID() const = 0;
    virtual void setAPSSID(const String& ssid) = 0;
    virtual String getAPPassword() const = 0;
    virtual void setAPPassword(const String& password) = 0;
    virtual bool getCheckUpdates() const = 0;
    virtual void setCheckUpdates(bool check) = 0;
    
    // Sensor configuration
    virtual float getMaxHeight() const = 0;
    virtual void setMaxHeight(float height) = 0;
    virtual float getCapacity() const = 0;
    virtual void setCapacity(float capacity) = 0;
    virtual float getMinDistance() const = 0;
    virtual void setMinDistance(float distance) = 0;
    
    // System configuration
    virtual bool getDisplayEnabled() const = 0;
    virtual void setDisplayEnabled(bool enabled) = 0;
    virtual uint8_t getBrightness() const = 0;
    virtual void setBrightness(uint8_t brightness) = 0;
    virtual uint16_t getUpdateInterval() const = 0;
    virtual void setUpdateInterval(uint16_t interval) = 0;
    virtual bool getDebugEnabled() const = 0;
    virtual void setDebugEnabled(bool enabled) = 0;
    
    // Import/Export
    virtual bool exportToJson(String& json_string) const = 0;
    virtual bool importFromJson(const String& json_string) = 0;
    
    // Error handling
    virtual String getLastError() const = 0;
};

#endif // ICONFIG_MANAGER_H