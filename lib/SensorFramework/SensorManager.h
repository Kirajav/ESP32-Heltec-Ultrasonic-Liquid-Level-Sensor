#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "Sensor.h"
#include <vector>

class SensorManager {
public:
    SensorManager();

    // Añade un sensor al manager. El manager no toma posesión del puntero.
    void addSensor(Sensor* sensor);

    // Llama al método begin() de todos los sensores registrados
    void begin();

    // Llama al método update() de todos los sensores registrados
    void update();

    // Obtiene el JSON de un sensor específico por su nombre
    String getSensorJson(const String& name) const;

    // Obtiene un JSON con los datos de todos los sensores
    String getAllSensorsJson() const;

private:
    std::vector<Sensor*> m_sensors;
};

#endif // SENSOR_MANAGER_H
