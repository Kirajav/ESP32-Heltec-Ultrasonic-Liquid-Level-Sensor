/**
 * Unit Tests - SensorConfig (Domain Config)
 * 
 * Testea la configuración del sensor: alturas, capacidad,
 * tipo de contenedor, intervalos de sensado, y serialización JSON.
 * 
 * Estas pruebas son lógica pura (sin hardware).
 * Se ejecutan en el ESP32 con: pio test -e heltec_test -f test_config
 */

#include <unity.h>
#include <Arduino.h>
#include "Domain/SensorConfig.h"

SensorConfig* sensorCfg = nullptr;

void setUp() {
    sensorCfg = new SensorConfig();
}

void tearDown() {
    delete sensorCfg;
    sensorCfg = nullptr;
}

// ============================================================
// Tests de valores por defecto
// ============================================================

void test_default_max_height() {
    // SensorConfig debe tener una altura máxima por defecto > 0
    double height = sensorCfg->getMaxHeight();
    TEST_ASSERT_GREATER_THAN(0, height);
}

void test_default_capacity() {
    // Capacidad por defecto debe ser > 0
    double capacity = sensorCfg->getCapacity();
    TEST_ASSERT_GREATER_THAN(0, capacity);
}

void test_default_container_type_is_tank() {
    // Tipo por defecto debe ser CONTAINER_TANK
    ContainerType type = sensorCfg->getContainerType();
    TEST_ASSERT_EQUAL(CONTAINER_TANK, type);
}

void test_default_min_distance() {
    // Distancia mínima por defecto debe ser >= 0
    double dist = sensorCfg->getMinDistance();
    TEST_ASSERT_GREATER_OR_EQUAL(0, dist);
}

void test_default_normal_interval() {
    // Intervalo normal por defecto = 30s
    uint16_t interval = sensorCfg->getNormalInterval();
    TEST_ASSERT_EQUAL(30, interval);
}

void test_default_filling_interval() {
    // Intervalo de llenado por defecto = 5s
    uint16_t interval = sensorCfg->getFillingInterval();
    TEST_ASSERT_EQUAL(5, interval);
}

// ============================================================
// Tests de setters/getters
// ============================================================

void test_set_max_height() {
    sensorCfg->setMaxHeight(200.0);
    TEST_ASSERT_EQUAL_DOUBLE(200.0, sensorCfg->getMaxHeight());
}

void test_set_capacity() {
    sensorCfg->setCapacity(1100.0);
    TEST_ASSERT_EQUAL_DOUBLE(1100.0, sensorCfg->getCapacity());
}

void test_set_min_distance() {
    sensorCfg->setMinDistance(5.0);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, sensorCfg->getMinDistance());
}

void test_set_container_type_cistern() {
    sensorCfg->setContainerType(CONTAINER_CISTERN);
    TEST_ASSERT_EQUAL(CONTAINER_CISTERN, sensorCfg->getContainerType());
}

void test_set_container_type_generic() {
    sensorCfg->setContainerType(CONTAINER_GENERIC);
    TEST_ASSERT_EQUAL(CONTAINER_GENERIC, sensorCfg->getContainerType());
}

void test_set_container_type_by_uint8() {
    sensorCfg->setContainerType((uint8_t)1); // CONTAINER_CISTERN
    TEST_ASSERT_EQUAL(CONTAINER_CISTERN, sensorCfg->getContainerType());
}

void test_container_type_string() {
    sensorCfg->setContainerType(CONTAINER_TANK);
    const char* typeStr = sensorCfg->getContainerTypeString();
    TEST_ASSERT_NOT_NULL(typeStr);
    // Debe contener algo, no ser vacío
    TEST_ASSERT_GREATER_THAN(0, strlen(typeStr));
}

void test_container_type_as_int() {
    sensorCfg->setContainerType(CONTAINER_CISTERN);
    TEST_ASSERT_EQUAL(1, sensorCfg->getContainerTypeAsInt());
}

void test_set_normal_interval() {
    sensorCfg->setNormalInterval(60);
    TEST_ASSERT_EQUAL(60, sensorCfg->getNormalInterval());
}

void test_set_filling_interval() {
    sensorCfg->setFillingInterval(3);
    TEST_ASSERT_EQUAL(3, sensorCfg->getFillingInterval());
}

void test_set_filling_threshold() {
    sensorCfg->setFillingThreshold(5);
    TEST_ASSERT_EQUAL(5, sensorCfg->getFillingThreshold());
}

// ============================================================
// Tests de configuración geográfica
// ============================================================

void test_set_timezone() {
    sensorCfg->setTimezone("America/Mexico_City");
    TEST_ASSERT_EQUAL_STRING("America/Mexico_City", sensorCfg->getTimezone().c_str());
}

void test_set_country_code() {
    sensorCfg->setCountryCode("MX");
    TEST_ASSERT_EQUAL_STRING("MX", sensorCfg->getCountryCode().c_str());
}

void test_set_city() {
    sensorCfg->setCity("Guadalajara");
    TEST_ASSERT_EQUAL_STRING("Guadalajara", sensorCfg->getCity().c_str());
}

void test_set_show_datetime() {
    sensorCfg->setShowDateTime(false);
    TEST_ASSERT_FALSE(sensorCfg->getShowDateTime());
    sensorCfg->setShowDateTime(true);
    TEST_ASSERT_TRUE(sensorCfg->getShowDateTime());
}

// ============================================================
// Tests de draining_interval (v3.1.0)
// ============================================================

void test_default_draining_interval() {
    TEST_ASSERT_EQUAL(10, sensorCfg->getDrainingInterval());
}

void test_set_draining_interval() {
    sensorCfg->setDrainingInterval(15);
    TEST_ASSERT_EQUAL(15, sensorCfg->getDrainingInterval());
}

void test_draining_interval_validation_min() {
    // Debe aceptar el mínimo válido (3)
    sensorCfg->setDrainingInterval(3);
    TEST_ASSERT_EQUAL(3, sensorCfg->getDrainingInterval());
}

void test_draining_interval_validation_max() {
    // Debe aceptar el máximo válido (60)
    sensorCfg->setDrainingInterval(60);
    TEST_ASSERT_EQUAL(60, sensorCfg->getDrainingInterval());
}

// ============================================================
// Tests de adaptive_interval (v3.1.0)
// ============================================================

void test_default_adaptive_interval() {
    // Por defecto el modo adaptativo está habilitado
    TEST_ASSERT_TRUE(sensorCfg->getAdaptiveInterval());
}

void test_set_adaptive_interval_off() {
    sensorCfg->setAdaptiveInterval(false);
    TEST_ASSERT_FALSE(sensorCfg->getAdaptiveInterval());
}

void test_set_adaptive_interval_on() {
    sensorCfg->setAdaptiveInterval(false);
    sensorCfg->setAdaptiveInterval(true);
    TEST_ASSERT_TRUE(sensorCfg->getAdaptiveInterval());
}

// ============================================================
// Tests de intervalos recomendados por tipo/volumen (v3.1.0)
// ============================================================

void test_recommended_normal_tank_small() {
    sensorCfg->setContainerType(CONTAINER_TANK);
    sensorCfg->setCapacity(50.0);
    TEST_ASSERT_EQUAL(10, sensorCfg->getRecommendedNormalInterval());
}

void test_recommended_normal_tank_medium() {
    sensorCfg->setContainerType(CONTAINER_TANK);
    sensorCfg->setCapacity(300.0);
    TEST_ASSERT_EQUAL(20, sensorCfg->getRecommendedNormalInterval());
}

void test_recommended_normal_tank_large() {
    sensorCfg->setContainerType(CONTAINER_TANK);
    sensorCfg->setCapacity(800.0);
    TEST_ASSERT_EQUAL(30, sensorCfg->getRecommendedNormalInterval());
}

void test_recommended_normal_cistern_small() {
    sensorCfg->setContainerType(CONTAINER_CISTERN);
    sensorCfg->setCapacity(800.0);
    TEST_ASSERT_EQUAL(30, sensorCfg->getRecommendedNormalInterval());
}

void test_recommended_normal_cistern_large() {
    sensorCfg->setContainerType(CONTAINER_CISTERN);
    sensorCfg->setCapacity(7000.0);
    TEST_ASSERT_EQUAL(90, sensorCfg->getRecommendedNormalInterval());
}

void test_recommended_filling_tank() {
    sensorCfg->setContainerType(CONTAINER_TANK);
    sensorCfg->setCapacity(50.0);
    TEST_ASSERT_EQUAL(3, sensorCfg->getRecommendedFillingInterval());
}

void test_recommended_draining_cistern() {
    sensorCfg->setContainerType(CONTAINER_CISTERN);
    sensorCfg->setCapacity(3000.0);
    TEST_ASSERT_EQUAL(20, sensorCfg->getRecommendedDrainingInterval());
}

void test_apply_adaptive_defaults() {
    sensorCfg->setContainerType(CONTAINER_CISTERN);
    sensorCfg->setCapacity(2000.0);
    sensorCfg->applyAdaptiveDefaults();
    
    TEST_ASSERT_EQUAL(45, sensorCfg->getNormalInterval());
    TEST_ASSERT_EQUAL(5, sensorCfg->getFillingInterval());
    TEST_ASSERT_EQUAL(10, sensorCfg->getDrainingInterval());
}

void test_recommended_generic_small() {
    sensorCfg->setContainerType(CONTAINER_GENERIC);
    sensorCfg->setCapacity(100.0);
    TEST_ASSERT_EQUAL(15, sensorCfg->getRecommendedNormalInterval());
}

// ============================================================
// Tests de FlowState enum (v3.1.0)
// ============================================================

void test_flow_state_values() {
    TEST_ASSERT_EQUAL(0, FLOW_IDLE);
    TEST_ASSERT_EQUAL(1, FLOW_FILLING);
    TEST_ASSERT_EQUAL(2, FLOW_DRAINING);
}

// ============================================================
// Tests de SensorModel (v3.1.0)
// ============================================================

void test_default_sensor_model() {
    TEST_ASSERT_EQUAL(SENSOR_AJ_SR04M, sensorCfg->getSensorModel());
}

void test_set_sensor_model() {
    sensorCfg->setSensorModel(SENSOR_HC_SR04);
    TEST_ASSERT_EQUAL(SENSOR_HC_SR04, sensorCfg->getSensorModel());
}

void test_set_sensor_model_by_uint8() {
    sensorCfg->setSensorModel((uint8_t)1); // SENSOR_HC_SR04
    TEST_ASSERT_EQUAL(SENSOR_HC_SR04, sensorCfg->getSensorModel());
}

void test_sensor_model_name_not_null() {
    const char* name = sensorCfg->getSensorModelName();
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_GREATER_THAN(0, strlen(name));
}

void test_apply_sensor_model_defaults() {
    sensorCfg->setSensorModel(SENSOR_HC_SR04);
    sensorCfg->applySensorModelDefaults();
    // HC-SR04: 2-400cm
    TEST_ASSERT_EQUAL_DOUBLE(2.0, sensorCfg->getMinDistance());
    TEST_ASSERT_EQUAL_DOUBLE(400.0, sensorCfg->getMaxDistance());
}

// ============================================================
// Tests de serialización JSON
// ============================================================

void test_save_and_load_json() {
    // Configurar valores específicos
    sensorCfg->setMaxHeight(180.0);
    sensorCfg->setCapacity(2500.0);
    sensorCfg->setMinDistance(3.0);
    sensorCfg->setContainerType(CONTAINER_CISTERN);
    sensorCfg->setNormalInterval(45);
    sensorCfg->setDrainingInterval(12);
    sensorCfg->setAdaptiveInterval(false);
    
    // Serializar a JSON
    StaticJsonDocument<1024> doc;
    JsonObject obj = doc.to<JsonObject>();
    sensorCfg->saveToJson(obj);
    
    // Crear nueva instancia y cargar desde JSON
    SensorConfig loaded;
    bool result = loaded.loadFromJson(obj);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_DOUBLE(180.0, loaded.getMaxHeight());
    TEST_ASSERT_EQUAL_DOUBLE(2500.0, loaded.getCapacity());
    TEST_ASSERT_EQUAL_DOUBLE(3.0, loaded.getMinDistance());
    TEST_ASSERT_EQUAL(CONTAINER_CISTERN, loaded.getContainerType());
    TEST_ASSERT_EQUAL(45, loaded.getNormalInterval());
    TEST_ASSERT_EQUAL(12, loaded.getDrainingInterval());
    TEST_ASSERT_FALSE(loaded.getAdaptiveInterval());
}

void test_set_defaults_resets_values() {
    sensorCfg->setMaxHeight(999.0);
    sensorCfg->setCapacity(999.0);
    sensorCfg->setDefaults();
    
    // Después de reset, no debe tener 999
    TEST_ASSERT_NOT_EQUAL(999.0, sensorCfg->getMaxHeight());
    TEST_ASSERT_NOT_EQUAL(999.0, sensorCfg->getCapacity());
}

// ============================================================
// Tests de validación
// ============================================================

void test_valid_config() {
    // Config por defecto debe ser válida
    TEST_ASSERT_TRUE(sensorCfg->isValid());
}

// ============================================================
// Tests del filtro sigma (v3.3.0)
// ============================================================

void test_default_sigma_samples() {
    TEST_ASSERT_EQUAL(DEFAULT_SIGMA_SAMPLES, sensorCfg->getSigmaSamples());
}

void test_default_sigma_delay_ms() {
    TEST_ASSERT_EQUAL(DEFAULT_SIGMA_DELAY_MS, sensorCfg->getSigmaDelayMs());
}

void test_default_sigma_threshold() {
    TEST_ASSERT_FLOAT_WITHIN(0.01, DEFAULT_SIGMA_THRESHOLD, sensorCfg->getSigmaThreshold());
}

void test_default_sigma_min_valid() {
    TEST_ASSERT_EQUAL(DEFAULT_SIGMA_MIN_VALID, sensorCfg->getSigmaMinValid());
}

void test_set_sigma_samples() {
    sensorCfg->setSigmaSamples(20);
    TEST_ASSERT_EQUAL(20, sensorCfg->getSigmaSamples());
}

void test_set_sigma_samples_min_boundary() {
    sensorCfg->setSigmaSamples(5);
    TEST_ASSERT_EQUAL(5, sensorCfg->getSigmaSamples());
}

void test_set_sigma_samples_max_boundary() {
    sensorCfg->setSigmaSamples(30);
    TEST_ASSERT_EQUAL(30, sensorCfg->getSigmaSamples());
}

void test_set_sigma_samples_below_min_rejected() {
    uint8_t original = sensorCfg->getSigmaSamples();
    sensorCfg->setSigmaSamples(4);
    TEST_ASSERT_EQUAL(original, sensorCfg->getSigmaSamples());
}

void test_set_sigma_samples_above_max_rejected() {
    uint8_t original = sensorCfg->getSigmaSamples();
    sensorCfg->setSigmaSamples(31);
    TEST_ASSERT_EQUAL(original, sensorCfg->getSigmaSamples());
}

void test_set_sigma_delay_ms() {
    sensorCfg->setSigmaDelayMs(75);
    TEST_ASSERT_EQUAL(75, sensorCfg->getSigmaDelayMs());
}

void test_set_sigma_delay_below_min_rejected() {
    uint16_t original = sensorCfg->getSigmaDelayMs();
    sensorCfg->setSigmaDelayMs(5);
    TEST_ASSERT_EQUAL(original, sensorCfg->getSigmaDelayMs());
}

void test_set_sigma_delay_above_max_rejected() {
    uint16_t original = sensorCfg->getSigmaDelayMs();
    sensorCfg->setSigmaDelayMs(600);
    TEST_ASSERT_EQUAL(original, sensorCfg->getSigmaDelayMs());
}

void test_set_sigma_threshold() {
    sensorCfg->setSigmaThreshold(2.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 2.0f, sensorCfg->getSigmaThreshold());
}

void test_set_sigma_threshold_below_min_rejected() {
    float original = sensorCfg->getSigmaThreshold();
    sensorCfg->setSigmaThreshold(0.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.01, original, sensorCfg->getSigmaThreshold());
}

void test_set_sigma_threshold_above_max_rejected() {
    float original = sensorCfg->getSigmaThreshold();
    sensorCfg->setSigmaThreshold(3.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.01, original, sensorCfg->getSigmaThreshold());
}

void test_set_sigma_min_valid() {
    sensorCfg->setSigmaMinValid(10);
    TEST_ASSERT_EQUAL(10, sensorCfg->getSigmaMinValid());
}

void test_set_sigma_min_valid_below_min_rejected() {
    uint8_t original = sensorCfg->getSigmaMinValid();
    sensorCfg->setSigmaMinValid(2);
    TEST_ASSERT_EQUAL(original, sensorCfg->getSigmaMinValid());
}

void test_set_sigma_min_valid_above_max_rejected() {
    uint8_t original = sensorCfg->getSigmaMinValid();
    sensorCfg->setSigmaMinValid(25);
    TEST_ASSERT_EQUAL(original, sensorCfg->getSigmaMinValid());
}

// ============================================================
// MAIN
// ============================================================

void setup() {
    delay(2000); // Esperar estabilización del serial
    
    UNITY_BEGIN();
    
    // Defaults
    RUN_TEST(test_default_max_height);
    RUN_TEST(test_default_capacity);
    RUN_TEST(test_default_container_type_is_tank);
    RUN_TEST(test_default_min_distance);
    RUN_TEST(test_default_normal_interval);
    RUN_TEST(test_default_filling_interval);
    
    // Setters/Getters
    RUN_TEST(test_set_max_height);
    RUN_TEST(test_set_capacity);
    RUN_TEST(test_set_min_distance);
    RUN_TEST(test_set_container_type_cistern);
    RUN_TEST(test_set_container_type_generic);
    RUN_TEST(test_set_container_type_by_uint8);
    RUN_TEST(test_container_type_string);
    RUN_TEST(test_container_type_as_int);
    RUN_TEST(test_set_normal_interval);
    RUN_TEST(test_set_filling_interval);
    RUN_TEST(test_set_filling_threshold);
    
    // Geográfico
    RUN_TEST(test_set_timezone);
    RUN_TEST(test_set_country_code);
    RUN_TEST(test_set_city);
    RUN_TEST(test_set_show_datetime);
    
    // Draining interval (v3.1.0)
    RUN_TEST(test_default_draining_interval);
    RUN_TEST(test_set_draining_interval);
    RUN_TEST(test_draining_interval_validation_min);
    RUN_TEST(test_draining_interval_validation_max);
    
    // Adaptive interval (v3.1.0)
    RUN_TEST(test_default_adaptive_interval);
    RUN_TEST(test_set_adaptive_interval_off);
    RUN_TEST(test_set_adaptive_interval_on);
    
    // Intervalos recomendados (v3.1.0)
    RUN_TEST(test_recommended_normal_tank_small);
    RUN_TEST(test_recommended_normal_tank_medium);
    RUN_TEST(test_recommended_normal_tank_large);
    RUN_TEST(test_recommended_normal_cistern_small);
    RUN_TEST(test_recommended_normal_cistern_large);
    RUN_TEST(test_recommended_filling_tank);
    RUN_TEST(test_recommended_draining_cistern);
    RUN_TEST(test_apply_adaptive_defaults);
    RUN_TEST(test_recommended_generic_small);
    
    // FlowState enum (v3.1.0)
    RUN_TEST(test_flow_state_values);
    
    // SensorModel (v3.1.0)
    RUN_TEST(test_default_sensor_model);
    RUN_TEST(test_set_sensor_model);
    RUN_TEST(test_set_sensor_model_by_uint8);
    RUN_TEST(test_sensor_model_name_not_null);
    RUN_TEST(test_apply_sensor_model_defaults);
    
    // JSON
    RUN_TEST(test_save_and_load_json);
    RUN_TEST(test_set_defaults_resets_values);
    
    // Validación
    RUN_TEST(test_valid_config);
    
    // Filtro sigma (v3.3.0)
    RUN_TEST(test_default_sigma_samples);
    RUN_TEST(test_default_sigma_delay_ms);
    RUN_TEST(test_default_sigma_threshold);
    RUN_TEST(test_default_sigma_min_valid);
    RUN_TEST(test_set_sigma_samples);
    RUN_TEST(test_set_sigma_samples_min_boundary);
    RUN_TEST(test_set_sigma_samples_max_boundary);
    RUN_TEST(test_set_sigma_samples_below_min_rejected);
    RUN_TEST(test_set_sigma_samples_above_max_rejected);
    RUN_TEST(test_set_sigma_delay_ms);
    RUN_TEST(test_set_sigma_delay_below_min_rejected);
    RUN_TEST(test_set_sigma_delay_above_max_rejected);
    RUN_TEST(test_set_sigma_threshold);
    RUN_TEST(test_set_sigma_threshold_below_min_rejected);
    RUN_TEST(test_set_sigma_threshold_above_max_rejected);
    RUN_TEST(test_set_sigma_min_valid);
    RUN_TEST(test_set_sigma_min_valid_below_min_rejected);
    RUN_TEST(test_set_sigma_min_valid_above_max_rejected);
    
    UNITY_END();
}

void loop() {
    // Nada
}
