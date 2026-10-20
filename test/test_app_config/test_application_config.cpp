/**
 * Unit Tests - ApplicationConfig (Facade)
 * 
 * Testea la configuración general de la aplicación:
 * - Inicialización con SPIFFS
 * - Acceso a sub-configuraciones (Network, Sensor, System)
 * - Convenience methods (getters/setters)
 * - JSON import/export
 * - Save/Load persistencia
 * 
 * NOTA: Estos tests necesitan SPIFFS montado (ESP32 real).
 * Se ejecutan con: pio test -e heltec_test -f test_config
 */

#include <unity.h>
#include <Arduino.h>
#include "ApplicationConfig.h"

ApplicationConfig* config = nullptr;

void setUp() {
    config = new ApplicationConfig();
}

void tearDown() {
    delete config;
    config = nullptr;
}

// ============================================================
// Tests de inicialización
// ============================================================

void test_begin_initializes_spiffs() {
    bool result = config->begin();
    TEST_ASSERT_TRUE(result);
}

void test_config_valid_after_begin() {
    config->begin();
    TEST_ASSERT_TRUE(config->isValid());
}

// ============================================================
// Tests de convenience methods - Network
// ============================================================

void test_get_set_hostname() {
    config->setHostname("mi-sensor");
    TEST_ASSERT_EQUAL_STRING("mi-sensor", config->getHostname().c_str());
}

void test_get_set_ap_ssid() {
    config->setAPSSID("ESP32-Test");
    TEST_ASSERT_EQUAL_STRING("ESP32-Test", config->getAPSSID().c_str());
}

void test_get_set_ap_password() {
    config->setAPPassword("testpass");
    TEST_ASSERT_EQUAL_STRING("testpass", config->getAPPassword().c_str());
}

void test_get_set_check_updates() {
    config->setCheckUpdates(false);
    TEST_ASSERT_FALSE(config->getCheckUpdates());
    config->setCheckUpdates(true);
    TEST_ASSERT_TRUE(config->getCheckUpdates());
}

// ============================================================
// Tests de convenience methods - Sensor
// ============================================================

void test_get_set_max_height() {
    config->setMaxHeight(200.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 200.0, config->getMaxHeight());
}

void test_get_set_capacity() {
    config->setCapacity(1100.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 1100.0, config->getCapacity());
}

void test_get_set_min_distance() {
    config->setMinDistance(5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 5.0, config->getMinDistance());
}

void test_get_set_container_type() {
    config->setContainerType(CONTAINER_CISTERN);
    TEST_ASSERT_EQUAL(CONTAINER_CISTERN, config->getContainerType());
}

void test_get_container_type_string() {
    config->setContainerType(CONTAINER_TANK);
    String typeStr = config->getContainerTypeString();
    TEST_ASSERT_GREATER_THAN(0, typeStr.length());
}

// ============================================================
// Tests de convenience methods - System
// ============================================================

void test_get_set_display_enabled() {
    config->setDisplayEnabled(false);
    TEST_ASSERT_FALSE(config->getDisplayEnabled());
    config->setDisplayEnabled(true);
    TEST_ASSERT_TRUE(config->getDisplayEnabled());
}

void test_get_set_auto_sleep_time() {
    config->setAutoSleepTime(60);
    TEST_ASSERT_EQUAL(60, config->getAutoSleepTime());
}

void test_get_set_update_interval() {
    config->setUpdateInterval(15);
    TEST_ASSERT_EQUAL(15, config->getUpdateInterval());
}

void test_get_set_debug_enabled() {
    config->setDebugEnabled(true);
    TEST_ASSERT_TRUE(config->getDebugEnabled());
    config->setDebugEnabled(false);
    TEST_ASSERT_FALSE(config->getDebugEnabled());
}

// ============================================================
// Tests de acceso a sub-configuraciones
// ============================================================

void test_access_network_config() {
    NetworkConfig& net = config->getNetworkConfig();
    net.setHostname("direct-access");
    TEST_ASSERT_EQUAL_STRING("direct-access", config->getHostname().c_str());
}

void test_access_sensor_config() {
    SensorConfig& sensor = config->getSensorConfig();
    sensor.setMaxHeight(250.0);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 250.0, config->getMaxHeight());
}

void test_access_system_config() {
    HardwareBoardConfig& sys = config->getSystemConfig();
    sys.setAutoSleepTime(90);
    TEST_ASSERT_EQUAL(90, config->getAutoSleepTime());
}

void test_access_espnow_config() {
    ESPNowConfig& espnow = config->getESPNowConfig();
    espnow.setMasterMode(true);
    TEST_ASSERT_TRUE(espnow.isMaster());
}

// ============================================================
// Tests de JSON import/export
// ============================================================

void test_export_to_json() {
    config->setHostname("export-test");
    config->setMaxHeight(180.0f);
    
    String jsonStr;
    bool result = config->exportToJson(jsonStr);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_GREATER_THAN(0, jsonStr.length());
}

void test_import_from_json() {
    config->setHostname("import-test");
    config->setCapacity(2000.0f);
    
    String jsonStr;
    config->exportToJson(jsonStr);
    
    ApplicationConfig config2;
    bool result = config2.importFromJson(jsonStr);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("import-test", config2.getHostname().c_str());
    TEST_ASSERT_FLOAT_WITHIN(0.1, 2000.0, config2.getCapacity());
}

// ============================================================
// Tests de persistencia (requiere SPIFFS)
// ============================================================

void test_save_and_load() {
    config->begin();
    
    config->setHostname("persist-test");
    config->setMaxHeight(175.0f);
    config->setAutoSleepTime(45);
    
    bool saved = config->save();
    TEST_ASSERT_TRUE(saved);
    
    ApplicationConfig config2;
    config2.begin();
    bool loaded = config2.load();
    TEST_ASSERT_TRUE(loaded);
    
    TEST_ASSERT_EQUAL_STRING("persist-test", config2.getHostname().c_str());
    TEST_ASSERT_FLOAT_WITHIN(0.1, 175.0, config2.getMaxHeight());
    TEST_ASSERT_EQUAL(45, config2.getAutoSleepTime());
}

void test_reset_restores_defaults() {
    config->setHostname("will-be-reset");
    config->setCapacity(9999.0f);
    
    config->reset();
    
    TEST_ASSERT_TRUE(config->getHostname() != "will-be-reset");
    TEST_ASSERT_TRUE(config->getCapacity() != 9999.0f);
}

// ============================================================
// MAIN
// ============================================================

void setup() {
    delay(2000);
    
    UNITY_BEGIN();
    
    RUN_TEST(test_begin_initializes_spiffs);
    RUN_TEST(test_config_valid_after_begin);
    
    RUN_TEST(test_get_set_hostname);
    RUN_TEST(test_get_set_ap_ssid);
    RUN_TEST(test_get_set_ap_password);
    RUN_TEST(test_get_set_check_updates);
    
    RUN_TEST(test_get_set_max_height);
    RUN_TEST(test_get_set_capacity);
    RUN_TEST(test_get_set_min_distance);
    RUN_TEST(test_get_set_container_type);
    RUN_TEST(test_get_container_type_string);
    
    RUN_TEST(test_get_set_display_enabled);
    RUN_TEST(test_get_set_auto_sleep_time);
    RUN_TEST(test_get_set_update_interval);
    RUN_TEST(test_get_set_debug_enabled);

    RUN_TEST(test_access_network_config);
    RUN_TEST(test_access_sensor_config);
    RUN_TEST(test_access_system_config);
    RUN_TEST(test_access_espnow_config);
    
    RUN_TEST(test_export_to_json);
    RUN_TEST(test_import_from_json);
    
    RUN_TEST(test_save_and_load);
    RUN_TEST(test_reset_restores_defaults);
    
    UNITY_END();
}

void loop() {}
