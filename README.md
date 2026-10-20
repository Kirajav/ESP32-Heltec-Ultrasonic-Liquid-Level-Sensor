# ESP32 Heltec Ultrasonic Liquid Level Sensor

Sistema de monitoreo de nivel de líquido (tinacos, cisternas, contenedores) basado en ESP32, con dashboard web, portal cautivo de configuración, integración local con Home Assistant vía MQTT, red ESP-NOW entre sensores y actualizaciones OTA.

> 📖 **Documentación para desarrolladores:** ver la [Wiki del proyecto](https://github.com/Kirajav/ESP32-Heltec-Ultrasonic-Liquid-Level-Sensor/wiki) (módulos, funciones, API REST, MQTT, ESP-NOW).

## Características

- **Sensor ultrasónico** HC-SR04 (o JSN-SR04T / AJ-SR04M / US-100 / A02YYUW) con **filtro sigma**: 20 muestras, rechazo de ecos falsos (spike guard) y de valores fuera de μ±kσ
- **Detección de estabilidad** y failsafe anti-desbordamiento cuando el agua entra en la zona ciega
- **Intervalos adaptativos** y detección de llenado/vaciado
- **Bajo consumo**: 0.059 A a 5.2 V (0.31 W) en reposo, −63 % frente a 3.3.4, gracias a modem sleep WiFi, CPU a 80 MHz y loop con idle
- **Dashboard web** con visualización SVG animada (tanque, cisterna, IBC) e historial
- **Portal cautivo** multilenguaje (ES/EN/FR) para la configuración inicial
- **MQTT / Home Assistant** local con Auto-Discovery (sensores + switch "Bomba de Agua"), reconexión automática
- **ESP-NOW** entre unidades, con descubrimiento automático
- **Display OLED** 128x64 SSD1306 con auto-sleep
- **OTA** (firmware y filesystem) vía ElegantOTA, con reinicio automático
- **WebSerial** para depuración remota desde el navegador (`help` lista los comandos)
- **Hora NTP** con zona horaria configurable (México `CST6` por defecto, sin horario de verano)

## Hardware

| Componente | Modelo | Notas |
|---|---|---|
| Microcontrolador | Heltec WiFi Kit 32 (V1) | OLED integrado |
| Sensor | HC-SR04 (recomendado) | Mínimo ~2–5 cm, poco ruido. **No es impermeable** |
| Sensor alternativo | JSN-SR04T | Impermeable, pero zona ciega de ~20 cm |
| Alimentación | Cargador USB 5 V ≥ 1 A | |

### Conexión

| Sensor | Heltec | Notas |
|---|---|---|
| VCC | 5V | |
| GND | GND | |
| TRIG | GPIO 12 | Directo |
| ECHO | GPIO 13 | **Divisor de voltaje** 1 kΩ (serie) + 2 kΩ (a GND): 5 V → 3.3 V |

```text
ECHO ──[1 kΩ]──┬── GPIO 13
               │
            [2 kΩ]
               │
GND ───────────┴── GND
```

> En tinacos cerrados la condensación corroe el HC-SR04: protege su placa (barniz/conformal coating sin tapar la malla de los transductores) o déjala fuera del tinaco.

## Instalación rápida

Requisitos: [PlatformIO](https://platformio.org/) (extensión de VS Code o CLI) y cable USB para la primera instalación.

```bash
git clone https://github.com/Kirajav/ESP32-Heltec-Ultrasonic-Liquid-Level-Sensor.git
cd ESP32-Heltec-Ultrasonic-Liquid-Level-Sensor

# Firmware
pio run -e heltec_wifi_kit_32 --target upload

# Archivos web (LittleFS)
pio run -e heltec_wifi_kit_32 --target uploadfs
```

Primera configuración: conéctate a la red WiFi del ESP32 (`ESP32: Sensor de nivel de liquido`, contraseña `12345678`, cámbiala en el portal) y abre `http://192.168.1.1`. Para volver al portal más tarde: doble reset dentro de 10 s.

> ⚠️ La configuración (WiFi, MQTT, sensor) vive en LittleFS. Volver a subir el filesystem (`uploadfs` u OTA en modo *Filesystem*) **la borra**. Las actualizaciones normales solo necesitan el firmware.

Después de configurar WiFi:

| Página | URL |
|---|---|
| Dashboard | `http://[IP]/` |
| Actualización OTA | `http://[IP]/update` |
| WebSerial | `http://[IP]/webserial` |
| Administración ESP-NOW (Escanear) | `http://[IP]/espnow-manager` |
| Dashboard multi-sensor | `http://[IP]/espnow_dashboard` |

## Home Assistant (MQTT)

Configura el broker (IP, puerto, usuario y contraseña de Mosquitto) desde el dashboard. El dispositivo aparece solo gracias a Auto-Discovery.

| Topic | Dirección | Descripción |
|---|---|---|
| `nivel_tinaco/{mac}/state` | ESP32 → HA | Datos del sensor (JSON) |
| `nivel_tinaco/{mac}/cmd` | HA → ESP32 | `FILLING_ON` / `FILLING_OFF` |
| `nivel_tinaco/{mac}/pump_state` | ESP32 → HA | Estado de la bomba (`ON`/`OFF`) |
| `nivel_tinaco/{mac}/status` | ESP32 → HA | Disponibilidad (`online`/`offline`) |

Si el broker se cae (reinicio de HA), el ESP32 reintenta indefinidamente (backoff de 5 s a 60 s).

## ESP-NOW (varias unidades)

Las unidades deben estar en el **mismo canal WiFi**; conectarlas al mismo router lo garantiza.

1. Enciende las unidades. Durante los **primeros 3 minutos** cada una se anuncia (PING cada 5 s) y escucha.
2. Si una ya llevaba tiempo encendida, pulsa **Escanear** en su página ESP-NOW: abre otra ventana de 3 minutos.
3. Al encontrarse quedan como *peers*; desde entonces la radio se mantiene despierta para no perder paquetes (sube el consumo).

## Consumo

Promedios medidos con un FNIRSI FNB58 y su software oficial a 100 muestras/s (5.2 V, pantalla apagada). Las temperaturas se tomaron con termopar sobre el chip, a 21–23 °C de ambiente:

| Firmware | Corriente | Potencia | Chip ESP32 (ambiente ~22 °C) |
|---|---|---|---|
| ≤ 3.3.4 | 0.160 A | 0.83 W | 37.2 °C y subiendo |
| 3.4.0 | 0.059 A | 0.31 W | 29.1 °C estable |

El ahorro de energía WiFi (modem sleep) se desactiva solo con peers ESP-NOW o durante el descubrimiento; con la radio despierta el consumo es de 0.125 A. A cambio, el ping sube a ~150 ms, cosa que no afecta al sensor.

Perfil de corriente por proceso (arranque, lectura, ping, HTTP, OLED, ESP-NOW), temperaturas y datos crudos: [wiki: Consumo y perfil de corriente](https://github.com/Kirajav/ESP32-Heltec-Ultrasonic-Liquid-Level-Sensor/wiki/Consumo-y-Perfil-de-Corriente).

## API REST (resumen)

| Método | Endpoint | Descripción |
|---|---|---|
| GET | `/api/status` | Estado + diagnóstico (`temp_c`, `cpu_mhz`, `wifi_sleep`, `rssi`, `uptime_s`, `heap`, `reset_reason`, ESP-NOW) |
| GET | `/api/sensor-data` | Lectura actual (litros, distancia, %, estado) |
| GET | `/api/last-readings` | Muestras RAW del último filtro sigma (`rt`/`rr` = reintentos) |
| GET | `/api/sensor-history` | Últimas 60 lecturas |
| GET/POST | `/api/sensor/config` | Configuración del sensor, intervalos, zona horaria, filtro sigma |
| GET/POST | `/api/mqtt/config` | Configuración MQTT |
| POST | `/api/calibrate-bottom` | Calibrar fondo del contenedor vacío |
| POST | `/api/restart` | Reiniciar |
| GET | `/api/esp-now/status` | Estado ESP-NOW |
| POST | `/api/esp-now/scan` | Abrir ventana de descubrimiento (3 min) |

La lista completa está en la [Wiki](https://github.com/Kirajav/ESP32-Heltec-Ultrasonic-Liquid-Level-Sensor/wiki).

## Arquitectura

```
src/
├── main.cpp              # Punto de entrada
├── AppManager.h/cpp      # Coordinador: red, web server, lectura, MQTT, ESP-NOW
lib/
├── Config/               # Configuración por dominios (red, sensor, MQTT, ESP-NOW, hardware)
├── DisplayManager/       # OLED SSD1306 + botón PRG + auto-sleep
├── SensorFramework/      # LiquidLevelSensor (filtro sigma, estabilidad, litros)
├── ESPNowManager/        # Comunicación ESP-NOW entre unidades
├── HomeAssistantMQTT/    # MQTT + Auto-Discovery
├── Logger/               # UnifiedLogger + NTPTimeSync
└── Interfaces/           # ISensor, ILogger, IDisplayManager, IConfigManager
data/web/
├── captive_portal/       # Portal de configuración (src/ → minificado → .gz)
├── dashboard/            # Dashboard principal
├── esp_now/              # Administración ESP-NOW
└── espnow_dashboard/     # Dashboard multi-sensor
```

Los archivos web se minifican y comprimen automáticamente en cada build (`scripts/minify_web.py`). Solo se editan los de `data/web/*/src/`.

## Tests

```bash
pio test -e heltec_test -f test_sensor_config
pio test -e heltec_test -f test_network_config
pio test -e heltec_test -f test_app_config
```

Los tests de frontend (`test/test_web/*.html`) se abren en el navegador.

## Seguridad

Pensado para una **red local de confianza**: la página OTA (`/update`) no pide contraseña y la API no tiene autenticación. No lo expongas a Internet.

## Build actual (3.4.0)

```
RAM:   [==        ]  18.3% (60,064 / 327,680 bytes)
Flash: [========= ]  89.6% (1,409,837 / 1,572,864 bytes)
```

## Licencia

[MIT](LICENSE)
