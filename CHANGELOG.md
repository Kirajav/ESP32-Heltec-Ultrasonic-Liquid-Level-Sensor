# Changelog

Todos los cambios notables de este proyecto se documentan en este archivo.

El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.0.0/).

---

## [3.4.0] — 2026-10-19

Consumo y calentamiento reducidos ~72%, MQTT que se reconecta solo, hora correcta
para México y ESP-NOW que por fin empareja unidades.

### 🌡️ Consumo y calentamiento (medido con FNIRSI FNB58 y termopar)

| | Antes | Después |
|---|---|---|
| Corriente a 5.2 V (pantalla apagada) | 0.167 A | ~0.045 A |
| Potencia | 0.87 W | ~0.24 W |
| Chip ESP32 (ambiente ~22 °C) | 37 °C y subiendo | 29 °C estable |

- **`loop()` sin pausa**: `yield()` no deja correr al idle task, así que el Core 1
  giraba al 100%. Ahora `delay(10)` permite que la CPU duerma entre iteraciones
- **Modem sleep WiFi activado** (`WIFI_PS_MIN_MODEM`): antes `WiFi.setSleep(false)`
  mantenía la radio encendida siempre (~100 mA). Se desactiva solo si hay peers
  ESP-NOW o durante el descubrimiento. Costo: ping de ~145 ms en promedio
- **CPU a 80 MHz** (antes 240 MHz)

### 🏠 MQTT / Home Assistant

- **Fix: MQTT se rendía para siempre** tras 10 reintentos ("Máx reintentos") y HA
  dejaba de recibir datos hasta reiniciar el ESP32. Ahora reintenta indefinidamente
  con backoff de 5 s hasta 60 s
- **Seguridad: `GET /api/mqtt/config` ya no devuelve la contraseña** del broker (cualquiera
  en la LAN podía leerla). Devuelve `"password":""` y `hasPassword`; un `POST` con la
  contraseña vacía conserva la guardada

### 🕐 Hora / NTP

- **Fix: la hora iba 1 h adelantada** de abril a octubre. La zona por defecto usaba la
  regla de horario de verano de México abolida en 2022 (`CST6CDT,M4.1.0,M10.5.0`).
  Ahora es `CST6`
- **Fix: la zona horaria del dashboard nunca se aplicaba** (`NTPTimeSync::setTimezone`
  no estaba implementada). Ahora acepta nombres IANA comunes (`America/Mexico_City`,
  `America/Cancun`, `America/Tijuana`, `America/New_York`, `Europe/Madrid`…) o una
  regla POSIX, y se aplica en caliente al guardar

### 🔗 ESP-NOW

- **Fix: dos unidades nunca se descubrían.** Cada una solo transmitía si ya conocía un
  peer, y solo conocía peers al recibir algo: nadie transmitía primero. Ahora cada
  lectura se transmite siempre
- **Ventana de descubrimiento** de 3 min al arrancar y al pulsar "Escanear": radio
  despierta y PING broadcast cada 5 s; quien responde queda como peer
- **Fix: "Escanear" dejaba el modo promiscuo encendido** para siempre (no hacía falta)
- **Fix: el rol master/slave se sobrescribía en cada arranque**; ahora se respeta el
  rol guardado
- **Fix: las páginas web de ESP-NOW daban 404** (sus `index.html` no tenían ruta). Nuevas
  rutas `/espnow_dashboard` y `/espnow-manager`; `/dashboard` redirige a `/`

### 📏 Sensor

- **Primera lectura inmediata al arrancar** (antes esperaba un intervalo completo, 120 s,
  mostrando "Error"/"Leyendo sensor..." mientras tanto)
- **Fórmula única de litros** (`LiquidLevelSensor::getLitros()`): OLED, historial y
  ESP-NOW usaban `capacidad*(maxH-d)/maxH` y diferían ~20 L de HA y del dashboard
- **Reintento de muestra sin eco** tras 60 ms; `/api/last-readings` reporta `rt`
  (reintentos) y `rr` (recuperadas)

### 🔧 OTA y diagnóstico

- **Fix: el ESP32 no se reiniciaba tras un OTA** (`ElegantOTA.loop()` nunca se llamaba)
- `/api/status` agrega `temp_c` (sensor interno, aproximado), `cpu_mhz`, `wifi_sleep`,
  `espnow_discovery`, `espnow_peers`, `rssi`, `uptime_s`, `heap`, `reset_reason`
- WebSerial `status` muestra temperatura interna, MHz y estado de modem sleep

### 🛠️ Build

- `minify_web.py` genera `.gz` deterministas (`mtime=0`): ya no cambian en cada build
- Agregado archivo `LICENSE` (MIT)

---

## [3.3.4] — 2026-04-08

### 🐛 Fix: Intervalo de 120s reportaba cada ~55s (desbordamiento uint16)

- **Causa raíz**: `currentInterval` era `uint16_t` (máximo 65,535). Al calcular
  `120 * 1000 = 120,000`, desbordaba a `120,000 % 65,536 = 54,464` (~54s).
  Por eso 30s funcionaba bien (`30,000 < 65,535`) pero 120s no.
- **Fix**: Cambiado `uint16_t currentInterval` → `uint32_t currentInterval`
  (máximo ~4,294 segundos) y multiplicación con `1000UL` para evitar overflow
- Afectaba intervalos normales, de llenado y vaciado > 65s

---

## [3.3.3] — 2026-04-08

Integración MQTT con Home Assistant: switch virtual de bomba de agua, corrección de
feedback de estado, intervalo adaptativo en dashboard, y log de eventos de bomba.

### 🚰 Switch Virtual "Bomba de Agua" en Home Assistant

- Nuevo **Auto-Discovery** de switch `bomba_de_agua` en HA — aparece automáticamente
  como entidad del dispositivo Sensor Nivel Agua
- HA puede enviar `FILLING_ON` / `FILLING_OFF` al topic `nivel_tinaco/{mac}/cmd`
- Al recibir `FILLING_ON`, el sensor activa **modo llenado** (intervalo rápido de sensado)
- Al recibir `FILLING_OFF`, vuelve al **intervalo normal**
- Compatible con automatizaciones HA que ligan un Smart Switch físico con el sensor

### 🐛 Fix: Feedback de estado de bomba no llegaba a HA

- **Causa raíz**: `handleHACommand()` construía el topic de feedback con
  `getUniqueId().substring(14)`, pero el prefijo `"nivel_tinaco_"` tiene **13**
  caracteres. Esto cortaba el primer carácter del MAC, publicando ON/OFF en un
  topic incorrecto que HA nunca recibía
- **Fix**: Nuevo método `publishPumpState(bool)` en `HomeAssistantMQTT` que construye
  el topic desde `_stateTopic` (misma fuente que el discovery), eliminando el error
- Estado inicial `OFF` publicado al conectar MQTT

### 🔄 Intervalo Adaptativo en Dashboard

- Nuevo checkbox **"Detección adaptativa de flujo"** en la configuración del sensor
- `saveSensorConfig()` ahora envía `adaptive_interval` al ESP32
- API GET `/api/sensor/config` devuelve el campo `adaptive_interval`
- Fix: `setAdaptiveInterval()` ya no llama `applyAdaptiveDefaults()` automáticamente
- Fix: `loadFromJson()` carga el flag adaptativo directamente sin disparar overrides
- Fix: API POST solo aplica defaults adaptativos cuando se habilita SIN intervalo manual

### 📊 Log de Eventos de Bomba en Dashboard

- Nuevo campo `pumpActive` en la respuesta de `/api/sensor-data`
- Detección de cambio de estado en el polling del dashboard
- Entradas destacadas en el registro de mediciones:
  - 💧 **BOMBA ENCENDIDA vía MQTT** — fondo azul con borde
  - 🛑 **BOMBA APAGADA vía MQTT** — fondo naranja con borde

### 📁 Archivos Modificados

- `lib/HomeAssistantMQTT/HomeAssistantMQTT.h` — `publishPumpState()`, `publish()`, `getUniqueId()`
- `lib/HomeAssistantMQTT/HomeAssistantMQTT.cpp` — pump switch discovery, initial OFF, publishPumpState
- `lib/Config/Domain/SensorConfig.cpp` — fix adaptive interval loading/saving
- `src/AppManager.cpp` — handleHACommand, pumpActive en API, adaptive_interval en API
- `data/web/dashboard/src/dashboard.js` — adaptive checkbox, pump log entries

---

## [3.3.2] — 2026-04-08

Corrección de intervalos MQTT, lecturas RAW en dashboard, y configuración unificada.

### 🐛 Fix: MQTT reportaba cada 30s en vez de 120s

- **Causa raíz**: la detección de flujo (llenado/vaciado) tenía umbrales demasiado
  sensibles (`minShortDelta=0.3L`, `minLongDelta=2.0L`) — el ruido natural del sensor
  (~30L de variación entre ciclos) disparaba falsos positivos constantemente
- **Fix 1**: `DEFAULT_ADAPTIVE_INTERVAL = false` — la detección de flujo ahora está
  **desactivada por defecto**. El sensor siempre usa el intervalo normal (120s)
- **Fix 2**: Umbrales aumentados significativamente cuando adaptativo está activo:
  `minShortDelta = 5.0 × (window-1)` (era 0.3), `minLongDelta = 15.0L` (era 2.0)
- **Fix 3**: Detección de flujo ahora condicional a `getAdaptiveInterval() == true`

### 📊 Lecturas RAW en el Registro del Dashboard

- Nuevo endpoint `/api/last-readings` — devuelve las N lecturas RAW del último ciclo
  sigma con flags (O=OK, S=SPIKE, X=σ-rejected, E=ERROR), media, desviación estándar,
  y conteo de supervivientes
- Dashboard muestra **todas las lecturas individuales** de cada ciclo antes del
  RESULTADO: `🔬 20 lecturas → 18 válidas | 1 spike | 1 σ-rej | μ:76.4cm σ:0.31`
- Getters agregados a `LiquidLevelSensor`: `getLastRawSamples()`, `getLastSpikeFlags()`,
  `getLastSigmaFlags()`, `getLastMean()`, `getLastStdDev()`, etc.

### ⚙️ Configuración Unificada

- Secciones "Intervalos de Sensado" y "Filtro Estadístico (σ)" **fusionadas** en una
  sola sección: **"📡 Configuración de Medición"**
- Sub-secciones claras: "⏱️ Intervalos entre ciclos" y "📊 Lecturas por ciclo"
- Descripciones mejoradas explicando el flujo completo: ciclo → N lecturas → filtro σ → resultado

### 🕐 Formato de hora

- `formatUnixTime()` ahora incluye `.000` para consistencia visual con timestamps en vivo

---

## [3.3.1] — 2026-04-08

Mejoras en intervalos de sensado, formato de logs y descripciones de configuración.

### ⏱️ Intervalos de Sensado Actualizados

- **Ciclo normal**: 30s → **120s (2 minutos)** por defecto — MQTT/HA reciben una
  lectura cada 2 minutos en estado estable
- **Ciclo en llenado/vaciado**: 5s → **30s** por defecto — configurable hasta 120s
- **Rango ampliado**: normal hasta 600s (10 min), llenado/vaciado hasta 120s
- MQTT solo reporta el **resultado final** de cada ciclo, no lecturas intermedias

### 📋 Registro de Mediciones (Dashboard)

- **Eliminada la fecha** de cada entrada — solo muestra hora con milisegundos (HH:MM:SS.mmm)
- Cada entrada muestra etiqueta **◀ RESULTADO** para identificar el valor final calculado
- Formato: `14:30:45.123  25.3L | 100.0cm | 50% | Conectado ◀ RESULTADO`

### 📟 WebSerial Monitor

- **Eliminada la fecha** de las lecturas — solo hora con milisegundos
- Comando `monitor` muestra **todos los detalles**: lecturas RAW, valores descartados
  (spike/sigma), media, desviación estándar, supervivientes
- Resultado final marcado con **◀ RESULTADO**

### ⚙️ Configuración Mejorada (Dashboard)

- Sección "Intervalos de Sensado" con **descripciones claras** de cada campo
- Sección "Filtro Estadístico (σ)" con explicación de qué controla
- Valores por defecto actualizados en inputs del Dashboard y Portal Cautivo
- `NTPTimeSync::getTimeWithMsString()` — nueva función para hora con milisegundos

---

## [3.3.0] — 2025-07-24

Refactorización completa del algoritmo de medición: el antiguo promedio
recortado (trimmed mean, 5 muestras) fue reemplazado por un **filtro sigma**
estadístico (20 muestras por defecto) para eliminar ruido y ecos falsos del
sensor ultrasónico HC-SR04 en tanques cilíndricos.

### 📊 Filtro Sigma (reemplaza trimmed mean)

- **20 muestras por ciclo** (antes 5) con delay configurable entre cada una
- **300ms de delay** entre muestras para evitar reverberación ultrasónica en tanques
  cilíndricos (análisis acústico: 50ms era insuficiente para que se extingan las
  ondas reflejadas dentro de contenedores metálicos/polietileno)
- **Spike guard**: rechaza muestras que difieren >30cm de la última lectura válida
  (detecta ecos fantasma/multipath inmediatamente)
- **Spike guard deadlock protection**: si el spike guard rechaza la mayoría de
  muestras durante 3 ciclos consecutivos, resetea la referencia para aceptar
  el nuevo nivel real (ej: cambio >30cm por consumo masivo o llenado mientras
  el sistema estaba apagado)
- **Cálculo estadístico**: media (μ) y desviación estándar (σ) de muestras válidas
- **Rechazo sigma**: descarta muestras fuera de μ±1.5σ (configurable)
- **Mínimo de supervivientes**: requiere ≥10 muestras (configurable) tras el filtro
- **Fallback graceful**: si pocas sobreviven al filtro sigma, usa media pre-filtro
- Diagnóstico completo en WebSerial: muestra todas las muestras RAW, cuáles fueron
  rechazadas por spike/sigma, μ, σ, rango aceptado, y supervivientes

### ⚙️ Parámetros Configurables (Dashboard + Portal Cautivo)

- `sigma_samples` (5-30, default 20): muestras por ciclo de lectura
- `sigma_delay_ms` (10-500ms, default 300): pausa entre muestras (uint16_t)
- `sigma_threshold` (1.0-3.0, default 1.5): multiplicador sigma para rechazo
- `sigma_min_valid` (3-20, default 10): mínimo de muestras supervivientes
- Todos los parámetros con descripción en español, inglés y francés
- Persistencia en LittleFS `/config.json`

### 🧪 Tests Unitarios

- 15 nuevos tests para setters/getters/validación de parámetros sigma
- Tests de boundary: valores mínimos, máximos, y fuera de rango rechazados

---

## [3.2.1] — 2025-04-06

Auto-recuperación del portal cautivo, detección de inestabilidad mejorada,
histéresis de estabilidad, y nueva entidad Modo Instalación en Home Assistant.

### 🔄 Auto-Reboot del Portal Cautivo

- **Reinicio automático tras 5 minutos** sin clientes conectados si hay WiFi guardada
- Previene que el sensor se quede en modo portal cautivo tras un corte de luz
- El timer se renueva cada vez que un cliente se conecta al AP
- No aplica si el portal fue forzado por doble reset (intencional)

### 🛡️ Detección de Inestabilidad Mejorada

- **Umbral de rango reducido** de 15cm a **8cm** (`UNSTABLE_RANGE_CM`)
- **Nueva condición OR**: oscilaciones ≥4 con rango > 5cm también disparan inestabilidad
  (antes requería rango > 15cm AND oscilaciones ≥4 — no capturaba oscilaciones de 10cm)
- Constante `UNSTABLE_OSC_RANGE_CM = 5.0` para rango mínimo de oscilaciones

### 🛡️ Histéresis en Detección de Estabilidad

- **Cooldown de 5 lecturas estables consecutivas** — el flag `m_sensorUnstable` ya no se
  limpia con 2-3 lecturas estables accidentales entre oscilaciones multipath
- Nuevo contador `m_consecutiveStable` con constante `STABLE_COOLDOWN_COUNT = 5`
- Previene que "Llenando" alterne On/Off cada ~30s durante episodios de multipath

### 📡 Nueva Entidad: Modo Instalación

- **`binary_sensor.modo_instalacion`** (icon: mdi:wrench) vía MQTT Discovery
- Campo `modo_instalacion` añadido al JSON de MQTT (`publishSensorData`)
- La detección de flujo se desactiva durante modo instalación (`_installationMode`)
- Total entidades HA: **8** (antes 7)

---

## [3.2.0] — 2025-04-05

Detección de estabilidad del sensor ultrasónico, failsafe anti-desbordamiento,
y throttle inteligente del log del dashboard.

### 🛡️ Estabilidad del Sensor — Detección de Ecos Falsos (Multipath)

- **Nuevo detector de oscilación** — analiza historial circular de 10 lecturas para detectar
  ecos falsos por multipath (rebotes en paredes de contenedores cilíndricos)
- Detección basada en rango (max-min > 15cm) + oscilaciones (≥4 cambios de dirección con deadband 2cm)
- **Fail-safe anti-desbordamiento** — tras 10 lecturas inestables consecutivas, clampea a
  `minDistance` y reporta `"Lleno (seguridad)"`, evitando que la bomba siga llenando
- Transición inteligente: si pierde eco tras inestabilidad, también asume lleno
- Nuevos métodos: `isSensorStable()`, `isFailsafeActive()`
- Estado `"Inestable"` reportado en `getLastError()` cuando se detecta oscilación

### 📡 Integración Home Assistant — Sensor Estable

- **Nueva entidad `binary_sensor.sensor_inestable`** (device_class: problem) vía MQTT Discovery
- Campo `sensor_estable` añadido al JSON de MQTT (`publishSensorData`)
- Campo `sensorEstable` y `estadoSensor` dinámico en REST API `/api/sensor-data`
- El estadoSensor ahora refleja: "Conectado", "Inestable", "Lleno (seguridad)", "Error"

### 📊 Dashboard — Throttle Inteligente del Log

- **Throttle basado en `normal_interval`** — el "Registro de Mediciones" respeta el intervalo
  configurado del sensor en vez de logear cada 5 segundos del polling
- Corregido: Live Monitor ya no causa entradas fantasma cada 5s en el registro
- `addSensorLogEntry._minInterval` se configura desde `/api/sensor/config` al cargar

### 🔍 Trazabilidad de Lecturas

- **readingId** añadido al endpoint `/api/sensor-data` — contador monotónico que incrementa
  con cada lectura del sensor, permite deduplicación en el frontend

### 🧪 Tests

- 4 nuevos tests de estabilidad del sensor: estado inicial, post-update, getters, mensajes de error
- Total tests backend: **123** (antes 119)

### 🔧 Correcciones

- **Falsos positivos de llenado/vaciado** — la detección de flujo se desactiva automáticamente
  cuando el sensor está inestable, evitando que oscilaciones multipath activen `FLOW_FILLING`
  o `FLOW_DRAINING` espurios en Home Assistant

---

## [3.1.0] — 2025-06-19

Intervalos de lectura adaptativos inteligentes, detección de vaciado, corrección
del dashboard de intervalos, y unificación de versiones en todo el firmware.

### 🧠 Intervalos Adaptativos de Lectura

- **Nuevo sistema de intervalos inteligentes** — calcula automáticamente la frecuencia de
  lectura según el tipo de contenedor (tanque, cisterna, genérico) y su capacidad en litros
- **Tanques**: ≤100L=10s, ≤500L=20s, >500L=30s (normal); escalas proporcionales para llenado/vaciado
- **Cisternas**: ≤1000L=30s, ≤2500L=45s, ≤5000L=60s, >5000L=90s (normal)
- **Contenedores genéricos**: ≤200L=15s, ≤1000L=30s, ≤5000L=60s, >5000L=120s (normal)
- Nuevos campos en `SensorConfig`: `adaptive_interval`, `draining_interval`
- Métodos nuevos: `getRecommendedNormalInterval()`, `getRecommendedFillingInterval()`,
  `getRecommendedDrainingInterval()`, `applyAdaptiveDefaults()`
- Toggle de modo adaptativo en captive portal con preview de intervalos recomendados

### 🚿 Detección de Vaciado (Draining)

- **Nuevo `FlowState` enum**: `FLOW_IDLE`, `FLOW_FILLING`, `FLOW_DRAINING`
- **`detectFlowState()`** reemplaza `detectFilling()` — algoritmo simétrico que detecta
  tanto llenado (deltas positivos) como vaciado (deltas negativos) usando ventanas rápida y lenta
- Indicador `[VACIANDO]` en monitor serie durante vaciado activo
- Intervalo de lectura dedicado para estado de vaciado (más rápido que normal, más lento que llenado)

### 🔧 Corrección del Dashboard de Intervalos

- **Corregido campo de intervalo "fantasma"** — el input `update_interval` en la sección
  de sistema enviaba datos a un endpoint que los ignoraba completamente
- **Movida configuración de intervalos a sección de sensor** donde se conecta correctamente
  al endpoint `/api/sensor/config` que sí los procesa
- Nuevos inputs: intervalo normal, llenado y vaciado con modo manual y adaptativo
- Backend actualizado para recibir `draining_interval` y `adaptive_interval`

### 🏷️ Unificación de Versiones

- **Todas las versiones unificadas a v3.1.0** — antes existían:
  - OLED splash: "v2.0" → ahora "v3.1"
  - main.cpp banner: "v3.0" → ahora "v3.1"
  - API `/api/esp-now/info`: "1.0.0" → ahora "3.1.0"
  - Firmware ID: "ESP32_OTA_WebServer_v2.0" → ahora "ESP32_LiquidLevel_v3.1.0"

---

## [3.0.0] — 2025-03-01

Optimización masiva de rendimiento, integración Tuya MQTT con TLS, nuevo sensor
LiquidLevel, y mejoras en la interfaz web del dashboard.

### ⚡ Rendimiento — Correcciones críticas

#### WiFi y red
- **Desactivar WiFi modem sleep** (`WiFi.setSleep(false)`) en `startNormalMode()` — elimina
  pérdida de paquetes de 100-300ms mientras el radio dormía entre transmisiones
- **Reducción del timeout de conexión WiFi** de 70s a 20s max (20 intentos × 500ms + 2
  reintentos en DRD)
- **Prioridad del task Tuya MQTT reducida** de 1 a 0 en ambos `xTaskCreatePinnedToCore` — evita
  que TLS compita con WiFi/AsyncTCP en Core 0

#### Sensor — Eliminación de bloqueos en handlers HTTP
- **Eliminado `sensor_manager.update()` de TODOS los handlers HTTP** — cada llamada bloqueaba
  AsyncTCP por 60ms (5 muestras × 12ms). Los handlers ahora usan la última lectura válida en
  RAM vía `_liquidSensor->getDistance()` / `_liquidSensor->getPercentage()`
  - `handleToggleDisplay()`: 80ms → 0ms de bloqueo
  - `/api/auto-sleep` POST: 80ms → 0ms
  - `/api/installation-mode` POST: 80ms → 0ms
  - `/api/calibrate-bottom` POST: **1.2 SEGUNDOS** → 0ms (antes: 3×update + 2×delay(500) + JSON parse)
- **Eliminado `sensor_manager.update()` de handlers WebSerial** — misma optimización:
  - Comando `monitor`/`mon`: usa acceso directo al sensor, sin JSON roundtrip
  - Comando `instalar`/`install`: lectura directa desde RAM
  - Comando `calibrar`/`calibrate`: lectura instantánea (antes: 3×update + 2×delay(500) = 1.2s)
  - Comando `restart`: `delay(2000)` → `vTaskDelay(500ms)`

#### Sensor — Optimización del loop principal
- **`delay()` → `vTaskDelay()`** en `LiquidLevelSensor::measureAveraged()` — ya no bloquea
  el scheduler FreeRTOS durante las 5 muestras (60ms total)
- **Eliminado triple JSON serialize/parse roundtrip** en el loop del sensor — acceso directo
  a `_liquidSensor->getDistance()`, `getPercentage()`, `getLastError()`
- **Eliminado JSON roundtrip en `updateSensorDisplay()`** — acceso directo al sensor
- **Eliminado JSON roundtrip en modo instalación** — acceso directo
- **3 llamadas `yield()` añadidas** al main loop para darle tiempo al servidor web async
- **Reducido `Serial.printf`** en el hot loop (sin emojis para menor latencia Serial)

#### I2C y Display
- **I2C acelerado de 100kHz a 400kHz** (`Wire.setClock(400000)`) en DisplayManager
- **Splash screen reducido** de 3s a 1.5s en el arranque

### 🔌 Tuya MQTT — Nueva integración

- **Nueva librería `TuyaMQTT`** con soporte TLS nativo (`WiFiClientSecure`)
- Conexión a Tuya IoT Cloud vía MQTT sobre TLS (puerto 8883)
- Ejecución en task FreeRTOS separado (Core 0, prioridad 0, 12KB stack)
- Publicación automática de datos del sensor a Tuya DataPoints
- Configuración completa desde el dashboard web (device ID, secret, product ID)
- Reconfiguración hot-reload sin reiniciar (señalización entre task y main loop)
- Limpieza: eliminados módulos legacy `TuyaIntegration/`, `TuyaSDK/`, `TuyaDevice`
- Eliminados documentos obsoletos: `EJEMPLOS_TECNICOS_TUYA.md`,
  `ANALISIS_SEGURIDAD_TUYA_IoT.md`, `MANUAL_USUARIO_TUYA_IOT.md`

### 📡 ESP-NOW — Mejoras

- **Guard `hasPeers()`** en todas las funciones de broadcast (17 funciones) — evita envíos
  cuando no hay peers registrados
- Nuevo método `bool hasPeers()` en `ESPNowManager.h`
- Optimización de `handleSensorData()` con buffer `snprintf` en stack (sin String heap alloc)
- Mejoras en estabilidad de registro/desregistro de peers

### 🖥️ Sensor — Nuevo `LiquidLevelSensor`

- **Nuevo sensor `LiquidLevelSensor`** reemplaza `WaterLevelSensor` — API mejorada:
  - `getDistance()`, `getPercentage()`, `hasValidReading()`, `getLastError()`
  - `getLastMeasurementDetails()` para verbose output
  - `calculateLitros()` con fórmula basada en config (maxHeight, minDistance, capacity)
- Eliminados archivos legacy: `WaterLevelSensor.cpp`, `WaterLevelSensor.h`
- `NUM_SAMPLES=5`, `SAMPLE_DELAY_MS=12` (60ms total por medición promediada)

### 🎨 Dashboard web

- **SVG de bandera mexicana rediseñado** — tricolor correcto con águila/serpiente/nopal
- **IntersectionObserver** para scroll-based navigation highlighting (reemplaza scroll events)
- **Barra de progreso OLED** durante conexión WiFi
- **Información del portal** mostrada en OLED durante modo AP
- Panel de configuración Tuya MQTT en el dashboard
- Sección de auto-sleep toggle
- Modo instalación activable desde dashboard

### 🛠️ Infraestructura y build

- **Nuevos `library.json`** para todas las libs locales (Config, DisplayManager, ESPNowManager,
  HardwareBoardStatus, Interfaces, Logger, OTAUpdater, SensorFramework, WebManager)
- **`TuyaConfig`** (nuevo): `lib/Config/Domain/TuyaConfig.cpp/.h`
- **Script `esp32_uploader.py` mejorado** — v2.0 con auto-detección de puertos, barras de
  progreso, upload de firmware + SPIFFS secuencial
- **Script `minify_web.py` mejorado** — preserva archivos originales en `/src/`, genera
  minificados y `.gz` automáticamente
- **Tabla de particiones** (`partitions_custom.csv`) actualizada
- **`platformio.ini`** con nuevas deps: PubSubClient, WiFiClientSecure
- Archivos `.min.*` eliminados (ahora se usan `.gz` directamente)
- Build final: RAM 18.6% (60,980B), Flash 96.5% (1,518,001 / 1,572,864 bytes)

### 📄 Otros cambios

- `SensorConfig` actualizado con nuevos campos y métodos
- `ApplicationConfig` con soporte para configuración Tuya
- `NetworkConfig` con correcciones menores
- `NTPTimeSync` con mejoras de estabilidad
- `WebManager` actualizado para nuevas rutas
- `main.cpp` con ajustes de inicialización
- Tests actualizados para nuevo API de sensor

---

## [Anterior] — Commits previos en `feature/unit-testing-framework`

### a2abd63 — fix(testing): Rewrite all backend tests with correct APIs and fix compilation
### 2f89910 — feat(testing): Implementar framework completo de unit testing con 106 tests
### 843cc09 — feat(display): Implementar scroll horizontal, contador de clientes y optimización
### 717c20b — refactor: Reorganizar estructura web con carpetas /src
### 1003f6f — refactor: Mejorar sistema de minificación - preservar archivos originales
