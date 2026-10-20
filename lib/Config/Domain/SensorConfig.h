#ifndef SENSOR_CONFIG_H
#define SENSOR_CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>

enum ContainerType {
    CONTAINER_TANK = 0,     // Tanque (cubetas, tinacos pequeños)
    CONTAINER_CISTERN = 1,  // Cisterna (>500L, subterránea o elevada)
    CONTAINER_GENERIC = 2   // Contenedor genérico
};

// Estados de flujo detectados
enum FlowState {
    FLOW_IDLE = 0,      // Sin cambios significativos
    FLOW_FILLING = 1,   // Llenando (nivel subiendo)
    FLOW_DRAINING = 2   // Vaciando/usando agua (nivel bajando)
};

// Modelos de sensores ultrasónicos soportados
enum SensorModel {
    SENSOR_JSN_SR04T = 0,   // JSN-SR04T (resistente al agua, 20-600cm)
    SENSOR_HC_SR04 = 1,     // HC-SR04 (estándar, 2-400cm)
    SENSOR_AJ_SR04M = 2,    // AJ-SR04M (resistente al agua, 20-800cm)
    SENSOR_US_100 = 3,      // US-100 (compensación de temperatura, 2-450cm)
    SENSOR_A02YYUW = 4,     // A02YYUW (resistente al agua UART, 30-450cm)
    SENSOR_DYP_ME007 = 5,   // DYP-ME007 (resistente al agua, 30-350cm)
    SENSOR_CUSTOM = 6       // Personalizado (distancias manuales)
};

// Especificaciones de cada modelo de sensor
struct SensorModelSpec {
    SensorModel model;
    const char* name;
    double min_distance;    // cm
    double max_distance;    // cm
    bool waterproof;
};

// Tabla de especificaciones de sensores (declarada en .cpp)
extern const SensorModelSpec SENSOR_SPECS[];
extern const uint8_t SENSOR_SPECS_COUNT;

struct SensorSettings {
    double max_height;      // altura_max -> max_height (cm)
    double capacity;        // capacidad -> capacity (liters)
    double min_distance;    // distancia_min -> min_distance (cm)
    double max_distance;    // distancia máxima del sensor (cm)
    ContainerType container_type; // tipo_contenedor -> container_type
    SensorModel sensor_model;    // Modelo del sensor ultrasónico
    
    // Configuraciones de tiempo de sensado
    uint16_t normal_interval;     // Intervalo normal en segundos (15-300s dependiendo del contenedor)
    uint16_t filling_interval;    // Intervalo al llenar en segundos (3-10s recomendado)
    uint16_t draining_interval;   // Intervalo al vaciar en segundos (5-15s recomendado)
    uint8_t filling_threshold;    // Número de lecturas incrementales para detectar llenado (3 por defecto)
    bool adaptive_interval;       // Usar intervalo adaptativo basado en tipo/volumen de contenedor
    
    // Filtro Sigma (estadístico) para muestras
    uint8_t  sigma_samples;         // Muestras por ciclo (5-30, default 20)
    uint16_t sigma_delay_ms;        // ms entre muestras (10-500, default 300)
    float    sigma_threshold;       // Multiplicador σ para descarte (1.0-3.0, default 1.5)
    uint8_t  sigma_min_valid;       // Mín. muestras válidas para aceptar lectura (3-20, default 10)
    
    // Configuración geográfica para NTP
    String timezone;             // Zona horaria (ej: "America/Mexico_City")
    String country_code;         // Código país (ej: "MX")
    String city;                 // Ciudad (ej: "Mexico City")
    bool auto_geo_location;      // Detectar ubicación automáticamente
    bool show_datetime;          // Mostrar fecha/hora en pantallas
};

class SensorConfig {
public:
    SensorConfig();
    
    // Core functionality
    bool loadFromJson(const JsonObject& json);
    void saveToJson(JsonObject& json) const;
    void setDefaults();
    
    // Height management
    void setMaxHeight(double height);
    double getMaxHeight() const { return settings.max_height; }
    
    // Capacity management
    void setCapacity(double capacity);
    double getCapacity() const { return settings.capacity; }
    
    // Distance management
    void setMinDistance(double distance);
    double getMinDistance() const { return settings.min_distance; }
    void setMaxDistance(double distance);
    double getMaxDistance() const { return settings.max_distance; }
    
    // Sensor model management
    void setSensorModel(SensorModel model);
    void setSensorModel(uint8_t model);
    SensorModel getSensorModel() const { return settings.sensor_model; }
    uint8_t getSensorModelAsInt() const { return static_cast<uint8_t>(settings.sensor_model); }
    const char* getSensorModelName() const;
    void applySensorModelDefaults();  // Aplica min/max distance del modelo seleccionado
    
    // Container type management
    void setContainerType(ContainerType type);
    void setContainerType(uint8_t type);
    ContainerType getContainerType() const { return settings.container_type; }
    uint8_t getContainerTypeAsInt() const { return static_cast<uint8_t>(settings.container_type); }
    const char* getContainerTypeString() const;
    
    // Sensor timing management
    void setNormalInterval(uint16_t seconds);
    uint16_t getNormalInterval() const { return settings.normal_interval; }
    void setFillingInterval(uint16_t seconds);
    uint16_t getFillingInterval() const { return settings.filling_interval; }
    void setDrainingInterval(uint16_t seconds);
    uint16_t getDrainingInterval() const { return settings.draining_interval; }
    void setFillingThreshold(uint8_t threshold);
    uint8_t getFillingThreshold() const { return settings.filling_threshold; }
    void setAdaptiveInterval(bool enabled);
    bool getAdaptiveInterval() const { return settings.adaptive_interval; }
    
    // Sigma filter management
    void setSigmaSamples(uint8_t samples);
    uint8_t getSigmaSamples() const { return settings.sigma_samples; }
    void setSigmaDelayMs(uint16_t ms);
    uint16_t getSigmaDelayMs() const { return settings.sigma_delay_ms; }
    void setSigmaThreshold(float threshold);
    float getSigmaThreshold() const { return settings.sigma_threshold; }
    void setSigmaMinValid(uint8_t min);
    uint8_t getSigmaMinValid() const { return settings.sigma_min_valid; }
    
    // Smart adaptive interval: calcula intervalos recomendados según contenedor
    uint16_t getRecommendedNormalInterval() const;
    uint16_t getRecommendedFillingInterval() const;
    uint16_t getRecommendedDrainingInterval() const;
    void applyAdaptiveDefaults();  // Aplica intervalos recomendados según tipo/volumen
    
    // Geographic/NTP management
    void setTimezone(const String& timezone);
    String getTimezone() const { return settings.timezone; }
    void setCountryCode(const String& code);
    String getCountryCode() const { return settings.country_code; }
    void setCity(const String& city);
    String getCity() const { return settings.city; }
    void setAutoGeoLocation(bool enabled);
    bool getAutoGeoLocation() const { return settings.auto_geo_location; }
    void setShowDateTime(bool show);
    bool getShowDateTime() const { return settings.show_datetime; }
    
    // Access to settings
    const SensorSettings& getSettings() const { return settings; }
    
    // Validation
    bool isValid() const;
    
private:
    SensorSettings settings;
    
    // Default values
    static const double DEFAULT_MAX_HEIGHT;
    static const double DEFAULT_CAPACITY;
    static const double DEFAULT_MIN_DISTANCE;
    static const double DEFAULT_MAX_DISTANCE;
    static const ContainerType DEFAULT_CONTAINER_TYPE = CONTAINER_TANK;
    static const SensorModel DEFAULT_SENSOR_MODEL = SENSOR_AJ_SR04M;
    
    // Timing defaults
    static const uint16_t DEFAULT_NORMAL_INTERVAL = 120;   // 2 minutos normal
    static const uint16_t DEFAULT_FILLING_INTERVAL = 30;    // 30 segundos llenando
    static const uint16_t DEFAULT_DRAINING_INTERVAL = 30;   // 30 segundos vaciando
    static const uint8_t DEFAULT_FILLING_THRESHOLD = 3;    // 3 lecturas incrementales
    static const bool DEFAULT_ADAPTIVE_INTERVAL = false;   // Desactivado: intervalo fijo evita falsos positivos por ruido
    
    // Sigma filter defaults
    static const uint8_t DEFAULT_SIGMA_SAMPLES = 20;
    static const uint16_t DEFAULT_SIGMA_DELAY_MS = 300;
    static constexpr float DEFAULT_SIGMA_THRESHOLD = 1.5f;
    static const uint8_t DEFAULT_SIGMA_MIN_VALID = 10;
    
    // Geographic defaults
    static const String DEFAULT_TIMEZONE;
    static const String DEFAULT_COUNTRY_CODE;
    static const String DEFAULT_CITY;
    static const bool DEFAULT_AUTO_GEO = true;
    static const bool DEFAULT_SHOW_DATETIME = true;
};

#endif // SENSOR_CONFIG_H