#ifndef ISENSOR_H
#define ISENSOR_H

#include <Arduino.h>

/**
 * @brief Interface for all sensor implementations
 * 
 * This interface defines the contract that all sensors must follow.
 * It enables dependency injection, testing with mocks, and easy
 * replacement of sensor implementations.
 */
class ISensor {
public:
    virtual ~ISensor() = default;
    
    // Core sensor operations
    virtual bool begin() = 0;
    virtual void update() = 0;
    virtual bool isReady() const = 0;
    virtual bool hasValidReading() const = 0;
    
    // Data access
    virtual float getDistance() const = 0;
    virtual float getPercentage() const = 0;
    virtual String getValue() const = 0;
    
    // Sensor identification
    virtual String getSensorType() const = 0;
    virtual String getSensorId() const = 0;
    
    // Status and diagnostics
    virtual bool isConnected() const = 0;
    virtual String getLastError() const = 0;
    virtual unsigned long getLastReadingTime() const = 0;
    
    // Configuration
    virtual void setEnabled(bool enabled) = 0;
    virtual bool isEnabled() const = 0;
};

#endif // ISENSOR_H