# Unit Testing Framework - ESP32 Liquid Level Sensor

## Estructura de Tests

```
test/
├── README.md                                   # Este archivo
├── test_sensor_config/                          # Tests de SensorConfig (domain)
│   └── test_sensor_config.cpp                   # 46 tests
├── test_network_config/                         # Tests de NetworkConfig (domain)
│   └── test_network_config.cpp                  # 21 tests
├── test_app_config/                             # Tests de ApplicationConfig (facade)
│   └── test_application_config.cpp              # 22 tests
├── test_display/                                # Tests del display OLED
│   └── test_display_manager.cpp                 # 33 tests
├── test_sensor/                                 # Tests del sensor HC-SR04
│   └── test_water_level_sensor.cpp              # 19 tests
├── test_integration/                            # (vacío - tests futuros)
└── test_web/                                    # Tests frontend (JavaScript)
    ├── README.md
    ├── test_dashboard.html                      # 15 tests
    ├── test_captive_portal.html                 # 22 tests
    └── test_espnow_dashboard.html               # 28 tests
```

**Total: 206 tests** (141 backend C++ + 65 frontend JavaScript)

---

## Framework: PlatformIO Unity

Los tests de backend usan **Unity** (C test framework para embedded systems),
integrado nativamente en PlatformIO.

### Assertions principales:
```cpp
TEST_ASSERT_EQUAL(expected, actual)
TEST_ASSERT_TRUE(condition)
TEST_ASSERT_FALSE(condition)
TEST_ASSERT_NULL(pointer)
TEST_ASSERT_NOT_NULL(pointer)
TEST_ASSERT_EQUAL_STRING(expected, actual)
TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)
TEST_ASSERT_EQUAL_DOUBLE(expected, actual)
TEST_ASSERT_GREATER_THAN(threshold, actual)
TEST_IGNORE_MESSAGE("razón")                      // Saltar test
```

---

## Cómo Ejecutar

### Backend (C++ - requiere ESP32 conectado por USB)

```bash
# Compilar todos (sin subir ni ejecutar - no necesita ESP32)
pio test -e heltec_test --without-uploading --without-testing

# Compilar un test suite específico
pio test -e heltec_test --without-uploading --without-testing -f test_sensor_config

# Ejecutar todos en ESP32 (requiere hardware conectado)
pio test -e heltec_test

# Ejecutar suite específico en ESP32
pio test -e heltec_test -f test_display

# Con output detallado
pio test -e heltec_test -v
```

### Frontend (JavaScript - solo navegador)

```powershell
# Abrir en navegador
start test/test_web/test_dashboard.html
start test/test_web/test_captive_portal.html
start test/test_web/test_espnow_dashboard.html
```

Los tests se ejecutan automáticamente al cargar la página.

---

## Detalle de Tests Backend

### test_sensor_config (46 tests) - Lógica pura
- Valores por defecto (height, capacity, container type, intervals)
- Setters/getters para todos los campos
- Tipos de contenedor (tank, cistern, generic) con enum `ContainerType`
- Intervalos de sensado (normal, filling, draining, threshold)
- Configuración geográfica (timezone, country, city)
- Serialización JSON (saveToJson/loadFromJson roundtrip)
- Reset a defaults, validación
- **v3.1.0:** Draining interval (defaults, setter, validación min/max)
- **v3.1.0:** Adaptive interval (defaults, toggle on/off)
- **v3.1.0:** Intervalos recomendados por tipo/volumen (tank, cistern, generic)
- **v3.1.0:** applyAdaptiveDefaults() roundtrip
- **v3.1.0:** FlowState enum values
- **v3.1.0:** SensorModel (defaults, setter, name, applySensorModelDefaults)

### test_network_config (21 tests) - Lógica pura
- Valores por defecto (hostname, AP SSID)
- Gestión multi-WiFi (add, remove, find, clear, set default)
- Hostname y AP configuration
- Credenciales legacy (setWiFiCredentials)
- Serialización JSON roundtrip
- Reset a defaults, validación

### test_app_config (22 tests) - Requiere SPIFFS (ESP32)
- Inicialización con SPIFFS
- Convenience methods (network, sensor, system)
- Acceso directo a sub-configs (getNetworkConfig, getSensorConfig, etc.)
- ESPNow config access
- JSON export/import
- Persistencia save/load
- Reset a defaults

### test_display (33 tests) - API pública de DisplayManager
- Auto-sleep: configuración, ciclo sleep/wake, force sleep
- LED: on/off, fade effects
- Scroll text (no-crash tests)
- Display content rendering
- Partial updates

### test_sensor (19 tests) - LiquidLevelSensor
- Inicialización y begin()
- Config verification (pins, container params)
- Distancia y porcentaje (TEST_IGNORE si no hay sensor físico)
- JSON output validation (estructura y parseo)
- Interfaz ISensor (getName, getSensorType, etc.)
- Cambios de config en runtime

---

## Detalle de Tests Frontend

| Archivo | Tests | Cobertura |
|---|---|---|
| test_dashboard.html | 15 | Cálculos, formateo, UI states |
| test_captive_portal.html | 22 | Validación WiFi, redes, formularios |
| test_espnow_dashboard.html | 28 | MAC parsing, estados, RSSI |
| **Total** | **65** | |

---

## Notas Importantes

1. **Cada test suite va en su propio directorio** - PlatformIO Unity enlaza todos los .cpp de un directorio en un solo binario
2. **test_build_src = no** - Los tests no compilan src/ (evita dependencias pesadas como AsyncTCP)
3. **Tests de sensor con HC-SR04** - Usan `TEST_IGNORE_MESSAGE` si no hay sensor conectado
4. **DisplayManager tests** - Solo API pública (no acceso a miembros privados)

---

## Recursos

- [PlatformIO Unit Testing](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html)
- [Unity Test Framework](https://github.com/ThrowTheSwitch/Unity)
