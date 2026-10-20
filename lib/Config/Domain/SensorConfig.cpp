#include "SensorConfig.h"

// Default values
const double SensorConfig::DEFAULT_MAX_HEIGHT = 200.0;  // 200 cm
const double SensorConfig::DEFAULT_CAPACITY = 1000.0;   // 1000 liters
const double SensorConfig::DEFAULT_MIN_DISTANCE = 20.0; // 20 cm (AJ-SR04M default)
const double SensorConfig::DEFAULT_MAX_DISTANCE = 800.0; // 800 cm (AJ-SR04M default)

// Tabla de especificaciones de sensores ultrasónicos
const SensorModelSpec SENSOR_SPECS[] = {
    { SENSOR_JSN_SR04T, "JSN-SR04T",  20.0, 600.0, true  },
    { SENSOR_HC_SR04,   "HC-SR04",     2.0, 400.0, false },
    { SENSOR_AJ_SR04M,  "AJ-SR04M",   20.0, 800.0, true  },
    { SENSOR_US_100,    "US-100",      2.0, 450.0, false },
    { SENSOR_A02YYUW,   "A02YYUW",    30.0, 450.0, true  },
    { SENSOR_DYP_ME007, "DYP-ME007",  30.0, 350.0, true  },
    { SENSOR_CUSTOM,    "Personalizado", 2.0, 800.0, false }
};
const uint8_t SENSOR_SPECS_COUNT = sizeof(SENSOR_SPECS) / sizeof(SENSOR_SPECS[0]);

// Timing and geo defaults
const String SensorConfig::DEFAULT_TIMEZONE = "America/Mexico_City";
const String SensorConfig::DEFAULT_COUNTRY_CODE = "MX";
const String SensorConfig::DEFAULT_CITY = "Mexico City";

SensorConfig::SensorConfig() {
    setDefaults();
}

bool SensorConfig::loadFromJson(const JsonObject& json) {
    if (json.containsKey("altura_max") || json.containsKey("max_height")) {
        // Support both old and new key names for backward compatibility
        settings.max_height = json.containsKey("max_height") ? 
            json["max_height"].as<double>() : json["altura_max"].as<double>();
    }
    
    if (json.containsKey("capacidad") || json.containsKey("capacity")) {
        settings.capacity = json.containsKey("capacity") ? 
            json["capacity"].as<double>() : json["capacidad"].as<double>();
    }
    
    if (json.containsKey("distancia_min") || json.containsKey("min_distance")) {
        settings.min_distance = json.containsKey("min_distance") ? 
            json["min_distance"].as<double>() : json["distancia_min"].as<double>();
    }
    
    if (json.containsKey("tipo_contenedor") || json.containsKey("container_type")) {
        uint8_t type = json.containsKey("container_type") ? 
            json["container_type"].as<uint8_t>() : json["tipo_contenedor"].as<uint8_t>();
        setContainerType(type);
    }
    
    if (json.containsKey("sensor_model")) {
        setSensorModel(json["sensor_model"].as<uint8_t>());
    }
    if (json.containsKey("max_distance")) {
        setMaxDistance(json["max_distance"].as<double>());
    }
    
    // Cargar adaptive PRIMERO (directo, sin setter que dispare defaults)
    if (json.containsKey("adaptive_interval")) {
        settings.adaptive_interval = json["adaptive_interval"].as<bool>();
    }
    
    // Cargar configuraciones de tiempo (valores explícitos del usuario siempre ganan)
    if (json.containsKey("normal_interval")) {
        setNormalInterval(json["normal_interval"].as<uint16_t>());
    }
    if (json.containsKey("filling_interval")) {
        setFillingInterval(json["filling_interval"].as<uint16_t>());
    }
    if (json.containsKey("draining_interval")) {
        setDrainingInterval(json["draining_interval"].as<uint16_t>());
    }
    if (json.containsKey("filling_threshold")) {
        setFillingThreshold(json["filling_threshold"].as<uint8_t>());
    }
    
    // Sigma filter params
    if (json.containsKey("sigma_samples")) {
        setSigmaSamples(json["sigma_samples"].as<uint8_t>());
    }
    if (json.containsKey("sigma_delay_ms")) {
        setSigmaDelayMs(json["sigma_delay_ms"].as<uint16_t>());
    }
    if (json.containsKey("sigma_threshold")) {
        setSigmaThreshold(json["sigma_threshold"].as<float>());
    }
    if (json.containsKey("sigma_min_valid")) {
        setSigmaMinValid(json["sigma_min_valid"].as<uint8_t>());
    }
    
    // Cargar configuraciones geográficas
    if (json.containsKey("timezone")) {
        setTimezone(json["timezone"].as<String>());
    }
    if (json.containsKey("country_code")) {
        setCountryCode(json["country_code"].as<String>());
    }
    if (json.containsKey("city")) {
        setCity(json["city"].as<String>());
    }
    if (json.containsKey("auto_geo_location")) {
        setAutoGeoLocation(json["auto_geo_location"].as<bool>());
    }
    if (json.containsKey("show_datetime")) {
        setShowDateTime(json["show_datetime"].as<bool>());
    }
    
    return isValid();
}

void SensorConfig::saveToJson(JsonObject& json) const {
    // Use new English key names
    json["max_height"] = settings.max_height;
    json["capacity"] = settings.capacity;
    json["min_distance"] = settings.min_distance;
    json["container_type"] = static_cast<uint8_t>(settings.container_type);
    
    json["max_distance"] = settings.max_distance;
    json["sensor_model"] = static_cast<uint8_t>(settings.sensor_model);
    
    // Keep old Spanish keys for backward compatibility
    json["altura_max"] = settings.max_height;
    json["capacidad"] = settings.capacity;
    json["distancia_min"] = settings.min_distance;
    json["tipo_contenedor"] = static_cast<uint8_t>(settings.container_type);
    
    // Guardar configuraciones de tiempo
    json["normal_interval"] = settings.normal_interval;
    json["filling_interval"] = settings.filling_interval;
    json["draining_interval"] = settings.draining_interval;
    json["filling_threshold"] = settings.filling_threshold;
    json["adaptive_interval"] = settings.adaptive_interval;
    
    // Sigma filter params
    json["sigma_samples"] = settings.sigma_samples;
    json["sigma_delay_ms"] = settings.sigma_delay_ms;
    json["sigma_threshold"] = settings.sigma_threshold;
    json["sigma_min_valid"] = settings.sigma_min_valid;
    
    // Guardar configuraciones geográficas
    json["timezone"] = settings.timezone;
    json["country_code"] = settings.country_code;
    json["city"] = settings.city;
    json["auto_geo_location"] = settings.auto_geo_location;
    json["show_datetime"] = settings.show_datetime;
}

void SensorConfig::setDefaults() {
    settings.max_height = DEFAULT_MAX_HEIGHT;
    settings.capacity = DEFAULT_CAPACITY;
    settings.min_distance = DEFAULT_MIN_DISTANCE;
    settings.max_distance = DEFAULT_MAX_DISTANCE;
    settings.container_type = DEFAULT_CONTAINER_TYPE;
    settings.sensor_model = DEFAULT_SENSOR_MODEL;
    
    // Valores por defecto de tiempo
    settings.normal_interval = DEFAULT_NORMAL_INTERVAL;
    settings.filling_interval = DEFAULT_FILLING_INTERVAL;
    settings.draining_interval = DEFAULT_DRAINING_INTERVAL;
    settings.filling_threshold = DEFAULT_FILLING_THRESHOLD;
    settings.adaptive_interval = DEFAULT_ADAPTIVE_INTERVAL;
    
    // Sigma filter defaults
    settings.sigma_samples = DEFAULT_SIGMA_SAMPLES;
    settings.sigma_delay_ms = DEFAULT_SIGMA_DELAY_MS;
    settings.sigma_threshold = DEFAULT_SIGMA_THRESHOLD;
    settings.sigma_min_valid = DEFAULT_SIGMA_MIN_VALID;
    
    // Valores por defecto geográficos
    settings.timezone = DEFAULT_TIMEZONE;
    settings.country_code = DEFAULT_COUNTRY_CODE;
    settings.city = DEFAULT_CITY;
    settings.auto_geo_location = DEFAULT_AUTO_GEO;
    settings.show_datetime = DEFAULT_SHOW_DATETIME;
}

void SensorConfig::setMaxHeight(double height) {
    if (height > 0 && height <= 1000) { // Max 10 meters
        settings.max_height = height;
    }
}

void SensorConfig::setCapacity(double capacity) {
    if (capacity > 0 && capacity <= 100000) { // Max 100,000 liters
        settings.capacity = capacity;
    }
}

void SensorConfig::setMinDistance(double distance) {
    if (distance >= 0 && distance <= 100) { // Max 1 meter
        settings.min_distance = distance;
    }
}

void SensorConfig::setMaxDistance(double distance) {
    if (distance > 0 && distance <= 1000) { // Max 10 meters
        settings.max_distance = distance;
    }
}

void SensorConfig::setSensorModel(SensorModel model) {
    if (model >= SENSOR_JSN_SR04T && model <= SENSOR_CUSTOM) {
        settings.sensor_model = model;
    }
}

void SensorConfig::setSensorModel(uint8_t model) {
    setSensorModel(static_cast<SensorModel>(model));
}

const char* SensorConfig::getSensorModelName() const {
    for (uint8_t i = 0; i < SENSOR_SPECS_COUNT; i++) {
        if (SENSOR_SPECS[i].model == settings.sensor_model) {
            return SENSOR_SPECS[i].name;
        }
    }
    return "Desconocido";
}

void SensorConfig::applySensorModelDefaults() {
    if (settings.sensor_model == SENSOR_CUSTOM) return; // No aplicar para personalizado
    for (uint8_t i = 0; i < SENSOR_SPECS_COUNT; i++) {
        if (SENSOR_SPECS[i].model == settings.sensor_model) {
            settings.min_distance = SENSOR_SPECS[i].min_distance;
            settings.max_distance = SENSOR_SPECS[i].max_distance;
            break;
        }
    }
}

void SensorConfig::setContainerType(ContainerType type) {
    if (type >= CONTAINER_TANK && type <= CONTAINER_GENERIC) {
        settings.container_type = type;
    }
}

void SensorConfig::setContainerType(uint8_t type) {
    setContainerType(static_cast<ContainerType>(type));
}

const char* SensorConfig::getContainerTypeString() const {
    switch (settings.container_type) {
        case CONTAINER_TANK:
            return "Tanque";
        case CONTAINER_CISTERN:
            return "Cisterna";
        case CONTAINER_GENERIC:
            return "Contenedor";
        default:
            return "Desconocido";
    }
}

// Métodos para configuraciones de tiempo
void SensorConfig::setNormalInterval(uint16_t seconds) {
    if (seconds >= 15 && seconds <= 600) { // Entre 15 segundos y 10 minutos
        settings.normal_interval = seconds;
    }
}

void SensorConfig::setFillingInterval(uint16_t seconds) {
    if (seconds >= 5 && seconds <= 120) { // Entre 5 segundos y 2 minutos
        settings.filling_interval = seconds;
    }
}

void SensorConfig::setFillingThreshold(uint8_t threshold) {
    if (threshold >= 2 && threshold <= 10) { // Entre 2 y 10 lecturas
        settings.filling_threshold = threshold;
    }
}

void SensorConfig::setDrainingInterval(uint16_t seconds) {
    if (seconds >= 3 && seconds <= 60) { // Entre 3 y 60 segundos
        settings.draining_interval = seconds;
    }
}

void SensorConfig::setAdaptiveInterval(bool enabled) {
    settings.adaptive_interval = enabled;
    // NOTA: ya NO llama applyAdaptiveDefaults() automáticamente.
    // El caller decide si quiere aplicar defaults (API endpoint lo hace,
    // loadFromJson NO lo hace para respetar valores explícitos del usuario).
}

// ── Intervalos inteligentes basados en tipo de contenedor y volumen ──
// La lógica: contenedores grandes cambian lentamente, no necesitan lecturas frecuentes.
// Contenedores pequeños (cubetas) cambian rápido, necesitan lecturas más seguidas.
uint16_t SensorConfig::getRecommendedNormalInterval() const {
    double cap = settings.capacity;
    
    switch (settings.container_type) {
        case CONTAINER_TANK:
            // Tanques: tipicamente 50-500L
            if (cap <= 100) return 10;       // Cubeta/tinaco pequeño: 10s
            if (cap <= 500) return 20;       // Tinaco mediano: 20s
            return 30;                        // Tinaco grande: 30s
            
        case CONTAINER_CISTERN:
            // Cisternas: tipicamente 500-10000L
            if (cap <= 1000) return 30;      // Cisterna pequeña: 30s
            if (cap <= 2500) return 45;      // Cisterna mediana: 45s
            if (cap <= 5000) return 60;      // Cisterna grande: 1 min
            return 90;                        // Cisterna industrial: 1.5 min
            
        case CONTAINER_GENERIC:
        default:
            // Genérico: basado puramente en volumen
            if (cap <= 200) return 15;
            if (cap <= 1000) return 30;
            if (cap <= 5000) return 60;
            return 120;                       // Muy grande: 2 min
    }
}

uint16_t SensorConfig::getRecommendedFillingInterval() const {
    double cap = settings.capacity;
    
    // Al llenar queremos medir más seguido para detectar cuando se llena
    switch (settings.container_type) {
        case CONTAINER_TANK:
            if (cap <= 100) return 3;         // Cubeta: 3s
            if (cap <= 500) return 5;         // Tinaco: 5s
            return 8;                          // Grande: 8s
            
        case CONTAINER_CISTERN:
            if (cap <= 2500) return 5;        // Cisterna pequeña/mediana: 5s
            return 10;                         // Cisterna grande: 10s
            
        case CONTAINER_GENERIC:
        default:
            if (cap <= 500) return 5;
            return 8;
    }
}

uint16_t SensorConfig::getRecommendedDrainingInterval() const {
    double cap = settings.capacity;
    
    // Vaciando: intermedio entre normal y llenando
    switch (settings.container_type) {
        case CONTAINER_TANK:
            if (cap <= 100) return 5;         // Cubeta: 5s
            if (cap <= 500) return 10;        // Tinaco: 10s
            return 15;                         // Grande: 15s
            
        case CONTAINER_CISTERN:
            if (cap <= 2500) return 10;       // Cisterna pequeña: 10s
            return 20;                         // Cisterna grande: 20s
            
        case CONTAINER_GENERIC:
        default:
            if (cap <= 500) return 8;
            return 15;
    }
}

void SensorConfig::applyAdaptiveDefaults() {
    settings.normal_interval = getRecommendedNormalInterval();
    settings.filling_interval = getRecommendedFillingInterval();
    settings.draining_interval = getRecommendedDrainingInterval();
}

// Métodos para configuraciones geográficas
void SensorConfig::setTimezone(const String& timezone) {
    if (timezone.length() > 0 && timezone.length() <= 64) {
        settings.timezone = timezone;
    }
}

void SensorConfig::setCountryCode(const String& code) {
    if (code.length() == 2) { // Código ISO de 2 letras
        settings.country_code = code;
        settings.country_code.toUpperCase();
    }
}

void SensorConfig::setCity(const String& city) {
    if (city.length() > 0 && city.length() <= 64) {
        settings.city = city;
    }
}

void SensorConfig::setAutoGeoLocation(bool enabled) {
    settings.auto_geo_location = enabled;
}

void SensorConfig::setShowDateTime(bool show) {
    settings.show_datetime = show;
}

void SensorConfig::setSigmaSamples(uint8_t samples) {
    if (samples >= 5 && samples <= 30) {
        settings.sigma_samples = samples;
    }
}

void SensorConfig::setSigmaDelayMs(uint16_t delay_ms) {
    if (delay_ms >= 10 && delay_ms <= 500) {
        settings.sigma_delay_ms = delay_ms;
    }
}

void SensorConfig::setSigmaThreshold(float threshold) {
    if (threshold >= 1.0f && threshold <= 3.0f) {
        settings.sigma_threshold = threshold;
    }
}

void SensorConfig::setSigmaMinValid(uint8_t min_valid) {
    if (min_valid >= 3 && min_valid <= 20) {
        settings.sigma_min_valid = min_valid;
    }
}

bool SensorConfig::isValid() const {
    return settings.max_height > 0 &&
           settings.max_height <= 1000 &&
           settings.capacity > 0 &&
           settings.capacity <= 100000 &&
           settings.min_distance >= 0 &&
           settings.min_distance <= 100 &&
           settings.min_distance < settings.max_height &&
           settings.container_type >= CONTAINER_TANK &&
           settings.container_type <= CONTAINER_GENERIC &&
           settings.normal_interval >= 5 &&
           settings.normal_interval <= 300 &&
           settings.filling_interval >= 1 &&
           settings.filling_interval <= 30 &&
           settings.draining_interval >= 3 &&
           settings.draining_interval <= 60 &&
           settings.filling_threshold >= 2 &&
           settings.filling_threshold <= 10;
}