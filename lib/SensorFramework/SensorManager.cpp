#include "SensorManager.h"

SensorManager::SensorManager() {}

void SensorManager::addSensor(Sensor* sensor) {
    if (sensor) {
        m_sensors.push_back(sensor);
    }
}

void SensorManager::begin() {
    for (Sensor* sensor : m_sensors) {
        sensor->begin();
    }
}

void SensorManager::update() {
    for (Sensor* sensor : m_sensors) {
        sensor->update();
    }
}

String SensorManager::getSensorJson(const String& name) const {
    for (const Sensor* sensor : m_sensors) {
        if (sensor->getName().equalsIgnoreCase(name)) {
            return sensor->getJson();
        }
    }
    return "{\"error\":\"Sensor no encontrado\"}";
}

String SensorManager::getAllSensorsJson() const {
    StaticJsonDocument<1024> doc;
    JsonArray sensorsArray = doc.to<JsonArray>();

    for (const Sensor* sensor : m_sensors) {
        // Deserializar el JSON de cada sensor para añadirlo al array
        StaticJsonDocument<256> sensorDoc;
        deserializeJson(sensorDoc, sensor->getJson());
        sensorsArray.add(sensorDoc);
    }

    String output;
    serializeJson(doc, output);
    return output;
}

