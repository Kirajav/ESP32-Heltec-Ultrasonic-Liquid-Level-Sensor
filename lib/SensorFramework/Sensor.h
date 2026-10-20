#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "ISensor.h"

// Interfaz para cualquier tipo de sensor
class Sensor : public ISensor {
public:
    virtual ~Sensor() {}

    // ISensor interface implementation - begin() ahora retorna bool
    virtual bool begin() override = 0;
    
    // Métodos adicionales específicos de Sensor
    virtual void update() = 0;
    virtual String getName() const = 0;
    virtual String getJson() const = 0;
    
    // ISensor interface default implementations
    virtual bool isReady() const override { return true; }
    virtual bool hasValidReading() const override = 0;
    virtual float getDistance() const override = 0;
    virtual float getPercentage() const override = 0;
    virtual String getValue() const override { return getJson(); }
    virtual String getSensorType() const override { return getName(); }
    virtual String getSensorId() const override { return getName(); }
    virtual bool isConnected() const override { return true; }
    virtual String getLastError() const override { return ""; }
    virtual unsigned long getLastReadingTime() const override { return millis(); }
    virtual void setEnabled(bool enabled) override { /* Default implementation */ }
    virtual bool isEnabled() const override { return true; }
};

#endif // SENSOR_H
