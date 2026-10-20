/**
 * Unit Tests - NetworkConfig (Domain Config)
 * 
 * Testea la configuración de red: WiFi multi-red, hostname,
 * SoftAP, credenciales y serialización JSON.
 * 
 * Estas pruebas son lógica pura (sin hardware).
 * Se ejecutan en el ESP32 con: pio test -e heltec_test -f test_config
 */

#include <unity.h>
#include <Arduino.h>
#include "Domain/NetworkConfig.h"

NetworkConfig* netCfg = nullptr;

void setUp() {
    netCfg = new NetworkConfig();
}

void tearDown() {
    delete netCfg;
    netCfg = nullptr;
}

// ============================================================
// Tests de valores por defecto
// ============================================================

void test_default_hostname_not_empty() {
    String hostname = netCfg->getHostname();
    TEST_ASSERT_GREATER_THAN(0, hostname.length());
}

void test_default_ap_ssid_not_empty() {
    String ssid = netCfg->getApSSID();
    TEST_ASSERT_GREATER_THAN(0, ssid.length());
}

void test_default_check_updates_enabled() {
    TEST_ASSERT_TRUE(netCfg->getCheckUpdates());
}

void test_default_wifi_networks_empty() {
    // Al inicio no debe haber redes WiFi configuradas
    TEST_ASSERT_EQUAL(0, netCfg->getWiFiNetworkCount());
}

// ============================================================
// Tests de hostname y AP
// ============================================================

void test_set_hostname() {
    netCfg->setHostname("mi-esp32");
    TEST_ASSERT_EQUAL_STRING("mi-esp32", netCfg->getHostname().c_str());
}

void test_set_ap_ssid() {
    netCfg->setApSSID("ESP32-Config");
    TEST_ASSERT_EQUAL_STRING("ESP32-Config", netCfg->getApSSID().c_str());
}

void test_set_ap_password() {
    netCfg->setApPassword("12345678");
    TEST_ASSERT_EQUAL_STRING("12345678", netCfg->getApPassword().c_str());
}

// ============================================================
// Tests de gestión multi-WiFi
// ============================================================

void test_add_wifi_network() {
    bool result = netCfg->addWiFiNetwork("RedCasa", "password123", true);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, netCfg->getWiFiNetworkCount());
}

void test_add_multiple_wifi_networks() {
    netCfg->addWiFiNetwork("Red1", "pass1");
    netCfg->addWiFiNetwork("Red2", "pass2");
    netCfg->addWiFiNetwork("Red3", "pass3");
    TEST_ASSERT_EQUAL(3, netCfg->getWiFiNetworkCount());
}

void test_remove_wifi_network() {
    netCfg->addWiFiNetwork("RedA", "passA");
    netCfg->addWiFiNetwork("RedB", "passB");
    
    bool result = netCfg->removeWiFiNetwork("RedA");
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, netCfg->getWiFiNetworkCount());
}

void test_remove_nonexistent_network() {
    netCfg->addWiFiNetwork("RedA", "passA");
    
    bool result = netCfg->removeWiFiNetwork("NoExiste");
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(1, netCfg->getWiFiNetworkCount());
}

void test_set_default_wifi_network() {
    netCfg->addWiFiNetwork("Red1", "pass1");
    netCfg->addWiFiNetwork("Red2", "pass2", true);
    
    bool result = netCfg->setDefaultWiFiNetwork("Red1");
    TEST_ASSERT_TRUE(result);
    
    WiFiNetwork* def = netCfg->getDefaultWiFiNetwork();
    TEST_ASSERT_NOT_NULL(def);
    TEST_ASSERT_EQUAL_STRING("Red1", def->ssid.c_str());
}

void test_find_wifi_network() {
    netCfg->addWiFiNetwork("MiRed", "miPass123");
    
    WiFiNetwork* found = netCfg->findWiFiNetwork("MiRed");
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("MiRed", found->ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("miPass123", found->password.c_str());
}

void test_find_nonexistent_network_returns_null() {
    WiFiNetwork* found = netCfg->findWiFiNetwork("NoExiste");
    TEST_ASSERT_NULL(found);
}

void test_clear_wifi_networks() {
    netCfg->addWiFiNetwork("Red1", "pass1");
    netCfg->addWiFiNetwork("Red2", "pass2");
    
    netCfg->clearWiFiNetworks();
    TEST_ASSERT_EQUAL(0, netCfg->getWiFiNetworkCount());
}

// ============================================================
// Tests de credenciales legacy
// ============================================================

void test_set_wifi_credentials_legacy() {
    bool result = netCfg->setWiFiCredentials("MiRedWiFi", "password123");
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("MiRedWiFi", netCfg->getWiFiSSID().c_str());
    TEST_ASSERT_EQUAL_STRING("password123", netCfg->getWiFiPassword().c_str());
}

void test_set_check_updates() {
    netCfg->setCheckUpdates(false);
    TEST_ASSERT_FALSE(netCfg->getCheckUpdates());
    netCfg->setCheckUpdates(true);
    TEST_ASSERT_TRUE(netCfg->getCheckUpdates());
}

// ============================================================
// Tests de serialización JSON
// ============================================================

void test_save_and_load_json() {
    // Configurar
    netCfg->setHostname("test-esp32");
    netCfg->setApSSID("TestAP");
    netCfg->setApPassword("testpass");
    netCfg->addWiFiNetwork("Red1", "pass1", true);
    netCfg->addWiFiNetwork("Red2", "pass2");
    
    // Serializar
    StaticJsonDocument<1024> doc;
    JsonObject obj = doc.to<JsonObject>();
    netCfg->saveToJson(obj);
    
    // Cargar en nueva instancia
    NetworkConfig loaded;
    bool result = loaded.loadFromJson(obj);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("test-esp32", loaded.getHostname().c_str());
    TEST_ASSERT_EQUAL_STRING("TestAP", loaded.getApSSID().c_str());
}

void test_set_defaults_resets() {
    netCfg->setHostname("custom-name");
    netCfg->addWiFiNetwork("Red1", "pass1");
    
    netCfg->setDefaults();
    
    TEST_ASSERT_NOT_EQUAL(0, netCfg->getHostname().length());
    TEST_ASSERT_EQUAL(0, netCfg->getWiFiNetworkCount());
}

// ============================================================
// Tests de validación
// ============================================================

void test_valid_config() {
    TEST_ASSERT_TRUE(netCfg->isValid());
}

// ============================================================
// MAIN
// ============================================================

void setup() {
    delay(2000);
    
    UNITY_BEGIN();
    
    // Defaults
    RUN_TEST(test_default_hostname_not_empty);
    RUN_TEST(test_default_ap_ssid_not_empty);
    RUN_TEST(test_default_check_updates_enabled);
    RUN_TEST(test_default_wifi_networks_empty);
    
    // Hostname y AP
    RUN_TEST(test_set_hostname);
    RUN_TEST(test_set_ap_ssid);
    RUN_TEST(test_set_ap_password);
    
    // Multi-WiFi
    RUN_TEST(test_add_wifi_network);
    RUN_TEST(test_add_multiple_wifi_networks);
    RUN_TEST(test_remove_wifi_network);
    RUN_TEST(test_remove_nonexistent_network);
    RUN_TEST(test_set_default_wifi_network);
    RUN_TEST(test_find_wifi_network);
    RUN_TEST(test_find_nonexistent_network_returns_null);
    RUN_TEST(test_clear_wifi_networks);
    
    // Legacy credentials
    RUN_TEST(test_set_wifi_credentials_legacy);
    RUN_TEST(test_set_check_updates);
    
    // JSON
    RUN_TEST(test_save_and_load_json);
    RUN_TEST(test_set_defaults_resets);
    
    // Validación
    RUN_TEST(test_valid_config);
    
    UNITY_END();
}

void loop() {}
