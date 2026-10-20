# Resumen de Unit Tests - ESP32 Water Level Sensor

## Estado: Compilación verificada

Todos los 5 test suites de backend compilan correctamente con `pio test -e heltec_test --without-uploading --without-testing`.

## Tests Backend (C++ / Unity Framework)

| Test Suite | Archivo | Tests | Compilación | Tipo |
|---|---|---|---|---|
| test_sensor_config | test_sensor_config.cpp | 24 | PASSED | Lógica pura |
| test_network_config | test_network_config.cpp | 21 | PASSED | Lógica pura |
| test_app_config | test_application_config.cpp | 22 | PASSED | Requiere SPIFFS |
| test_display | test_display_manager.cpp | 33 | PASSED | Hardware (display) |
| test_sensor | test_water_level_sensor.cpp | 23 | PASSED | Hardware (HC-SR04) |
| **Total backend** | | **123** | | |

## Tests Frontend (JavaScript / HTML)

| Archivo | Tests | Ejecución |
|---|---|---|
| test_dashboard.html | 15 | Navegador |
| test_captive_portal.html | 22 | Navegador |
| test_espnow_dashboard.html | 28 | Navegador |
| **Total frontend** | **65** | |

## Total General: 188 tests

## Comandos

```bash
# Solo compilar (sin ESP32)
pio test -e heltec_test --without-uploading --without-testing

# Compilar un suite específico
pio test -e heltec_test --without-uploading --without-testing -f test_sensor_config

# Ejecutar en ESP32 (requiere hardware)
pio test -e heltec_test

# Frontend (navegador)
start test/test_web/test_dashboard.html
```
