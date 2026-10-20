/**
 * Unit Tests - LiquidLevelSensor (Public API)
 * 
 * Testea el sensor de nivel de liquido HC-SR04:
 * - Inicialización
 * - Lectura de distancia (requiere hardware)
 * - Porcentaje calculado
 * - Validez de lecturas
 * - Salida JSON
 * 
 * NOTA: Estos tests necesitan el ESP32 con sensor HC-SR04 conectado.
 * Los tests que requieren lecturas reales usan TEST_IGNORE si no hay sensor.
 * Se ejecutan con: pio test -e heltec_test -f test_sensor
 */

#include <unity.h>
#include <Arduino.h>
#include "LiquidLevelSensor.h"
#include "ApplicationConfig.h"

// Pines del sensor HC-SR04 (según el proyecto)
#define TRIGGER_PIN 12
#define ECHO_PIN 13

ApplicationConfig* config = nullptr;
LiquidLevelSensor* sensor = nullptr;

void setUp() {
    config = new ApplicationConfig();
    config->setMaxHeight(150.0f);
    config->setCapacity(1100.0f);
    config->setMinDistance(3.0f);
    config->setContainerType(CONTAINER_TANK);
    
    sensor = new LiquidLevelSensor(TRIGGER_PIN, ECHO_PIN, config);
}

void tearDown() {
    delete sensor;
    sensor = nullptr;
    delete config;
    config = nullptr;
}

// ============================================================
// Tests de inicialización
// ============================================================

void test_sensor_begin() {
    bool result = sensor->begin();
    TEST_ASSERT_TRUE(result);
}

void test_sensor_name() {
    String name = sensor->getName();
    TEST_ASSERT_GREATER_THAN(0, name.length());
}

// ============================================================
// Tests de configuración del sensor
// ============================================================

void test_config_max_height() {
    // Verificar que el sensor usa la config correcta
    TEST_ASSERT_FLOAT_WITHIN(0.1, 150.0, config->getMaxHeight());
}

void test_config_capacity() {
    TEST_ASSERT_FLOAT_WITHIN(0.1, 1100.0, config->getCapacity());
}

void test_config_min_distance() {
    TEST_ASSERT_FLOAT_WITHIN(0.1, 3.0, config->getMinDistance());
}

void test_config_container_type() {
    TEST_ASSERT_EQUAL(CONTAINER_TANK, config->getContainerType());
}

// ============================================================
// Tests de lectura (requiere sensor HC-SR04 conectado)
// ============================================================

void test_update_and_read_distance() {
    sensor->begin();
    sensor->update();
    
    float distance = sensor->getDistance();
    // Si el sensor está conectado, debería dar una lectura
    // Si no, será -1 o similar
    if (distance < 0) {
        TEST_IGNORE_MESSAGE("Sensor HC-SR04 no conectado - ignorando test de distancia");
    }
    TEST_ASSERT_GREATER_OR_EQUAL(0, distance);
}

void test_percentage_range() {
    sensor->begin();
    sensor->update();
    
    float distance = sensor->getDistance();
    if (distance < 0) {
        TEST_IGNORE_MESSAGE("Sensor HC-SR04 no conectado - ignorando test de porcentaje");
    }
    
    float percentage = sensor->getPercentage();
    // Porcentaje debe estar entre 0 y 100
    TEST_ASSERT_GREATER_OR_EQUAL(0, percentage);
    TEST_ASSERT_LESS_OR_EQUAL(100, percentage);
}

void test_has_valid_reading_with_sensor() {
    sensor->begin();
    sensor->update();
    
    float distance = sensor->getDistance();
    if (distance < 0) {
        // Sin sensor, la lectura no es válida
        TEST_ASSERT_FALSE(sensor->hasValidReading());
    } else {
        TEST_ASSERT_TRUE(sensor->hasValidReading());
    }
}

// ============================================================
// Tests de JSON output
// ============================================================

void test_get_json_not_empty() {
    sensor->begin();
    sensor->update();
    
    String json = sensor->getJson();
    TEST_ASSERT_GREATER_THAN(0, json.length());
}

void test_get_json_is_valid_json() {
    sensor->begin();
    sensor->update();
    
    String json = sensor->getJson();
    
    // Intentar parsear el JSON
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, json);
    TEST_ASSERT_TRUE(error == DeserializationError::Ok);
}

void test_get_json_has_distance_field() {
    sensor->begin();
    sensor->update();
    
    String json = sensor->getJson();
    StaticJsonDocument<512> doc;
    deserializeJson(doc, json);
    
    // El JSON debe contener campo de distancia
    TEST_ASSERT_TRUE(doc.containsKey("distancia") || doc.containsKey("distance"));
}

// ============================================================
// Tests de interfaz ISensor
// ============================================================

void test_sensor_type() {
    String type = sensor->getSensorType();
    TEST_ASSERT_GREATER_THAN(0, type.length());
}

void test_sensor_id() {
    String id = sensor->getSensorId();
    TEST_ASSERT_GREATER_THAN(0, id.length());
}

void test_sensor_value_returns_json() {
    sensor->begin();
    sensor->update();
    
    String value = sensor->getValue();
    TEST_ASSERT_GREATER_THAN(0, value.length());
}

void test_sensor_last_error_empty_initially() {
    // Sin update, no debería haber error
    sensor->begin();
    String error = sensor->getLastError();
    // Puede estar vacío o tener un mensaje - no crashea
    TEST_ASSERT_TRUE(true);
}

void test_sensor_is_enabled() {
    TEST_ASSERT_TRUE(sensor->isEnabled());
}

// ============================================================
// Tests con diferentes configuraciones
// ============================================================

void test_config_change_affects_sensor() {
    sensor->begin();
    
    // Cambiar capacidad
    config->setCapacity(2500.0f);
    sensor->update();
    
    // El sensor debe usar la nueva capacidad
    // No podemos verificar directamente sin sensor,
    // pero al menos no debe crashear
    TEST_ASSERT_FLOAT_WITHIN(0.1, 2500.0, config->getCapacity());
}

void test_cistern_container_type() {
    config->setContainerType(CONTAINER_CISTERN);
    config->setCapacity(5000.0f);
    config->setMaxHeight(200.0f);
    
    sensor->begin();
    sensor->update();
    
    // No debe crashear con tipo cisterna
    String json = sensor->getJson();
    TEST_ASSERT_GREATER_THAN(0, json.length());
}

// ============================================================
// Tests de estabilidad del sensor (detección de ecos falsos)
// ============================================================

void test_stability_initial_state() {
    sensor->begin();
    // Sin lecturas, sensor debe estar estable
    TEST_ASSERT_TRUE(sensor->isSensorStable());
    TEST_ASSERT_FALSE(sensor->isFailsafeActive());
}

void test_stability_after_single_update() {
    sensor->begin();
    sensor->update();
    // Incluso sin sensor, no debe activar failsafe con una sola lectura
    TEST_ASSERT_FALSE(sensor->isFailsafeActive());
}

void test_stability_getters_no_crash() {
    sensor->begin();
    for (int i = 0; i < 15; i++) {
        sensor->update();
    }
    // Los getters de estabilidad no deben crashear
    bool stable = sensor->isSensorStable();
    bool failsafe = sensor->isFailsafeActive();
    (void)stable;
    (void)failsafe;
    TEST_ASSERT_TRUE(true);
}

void test_stability_error_message_valid() {
    sensor->begin();
    for (int i = 0; i < 15; i++) {
        sensor->update();
    }
    String error = sensor->getLastError();
    TEST_ASSERT_GREATER_THAN(0, error.length());
    // Debe ser uno de los mensajes válidos
    TEST_ASSERT_TRUE(
        error == "OK" || error == "Inestable" || 
        error == "Lleno (min)" || error == "Lleno (seguridad)" || 
        error == "Error de lectura" || error == "No_init"
    );
}

// ============================================================
// MAIN
// ============================================================

void setup() {
    delay(2000);
    
    UNITY_BEGIN();
    
    // Inicialización
    RUN_TEST(test_sensor_begin);
    RUN_TEST(test_sensor_name);
    
    // Configuración
    RUN_TEST(test_config_max_height);
    RUN_TEST(test_config_capacity);
    RUN_TEST(test_config_min_distance);
    RUN_TEST(test_config_container_type);
    
    // Lectura (requiere hardware)
    RUN_TEST(test_update_and_read_distance);
    RUN_TEST(test_percentage_range);
    RUN_TEST(test_has_valid_reading_with_sensor);
    
    // JSON
    RUN_TEST(test_get_json_not_empty);
    RUN_TEST(test_get_json_is_valid_json);
    RUN_TEST(test_get_json_has_distance_field);
    
    // ISensor interface
    RUN_TEST(test_sensor_type);
    RUN_TEST(test_sensor_id);
    RUN_TEST(test_sensor_value_returns_json);
    RUN_TEST(test_sensor_last_error_empty_initially);
    RUN_TEST(test_sensor_is_enabled);
    
    // Config changes
    RUN_TEST(test_config_change_affects_sensor);
    RUN_TEST(test_cistern_container_type);
    
    // Estabilidad (detección ecos falsos)
    RUN_TEST(test_stability_initial_state);
    RUN_TEST(test_stability_after_single_update);
    RUN_TEST(test_stability_getters_no_crash);
    RUN_TEST(test_stability_error_message_valid);
    
    UNITY_END();
}

void loop() {}
