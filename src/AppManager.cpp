#include "AppManager.h"
#include <DNSServer.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>
// EEPROM removido - DRD ahora usa LittleFS (no hay partición eeprom en la tabla)

AsyncWebServer server(80);
DNSServer dnsServer;
DoubleResetDetector drd(10, 0);  // 10 segundos timeout para doble reset

// Variables para estabilidad del sistema
static unsigned long lastWiFiScan = 0;
static const unsigned long WIFI_SCAN_INTERVAL = 3000; // 3 segundos mínimo entre escaneos

// Código de download mode removido - no es necesario

// Página /instalacion: distancia cruda ~1 lectura/s, sin dependencias externas
static const char INSTALL_PAGE_HTML[] PROGMEM = R"HTML(<!doctype html><html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Modo instalación</title><style>
:root{--bg:#fcfcfb;--fg:#0b0b0b;--mut:#52514e;--acc:#2a78d6;--bd:#e1e0d9}
@media(prefers-color-scheme:dark){:root{--bg:#1a1a19;--fg:#f0efec;--mut:#c3c2b7;--acc:#3987e5;--bd:#2c2c2a}}
body{margin:0;padding:16px;font-family:system-ui,sans-serif;background:var(--bg);color:var(--fg)}
h1{font-size:18px;margin:0 0 4px}.m{color:var(--mut);font-size:13px}
#d{font-size:64px;font-weight:600;margin:16px 0 4px;font-variant-numeric:tabular-nums}
button{font-size:16px;padding:10px 14px;margin:8px 8px 0 0;border-radius:8px;border:1px solid var(--bd);background:var(--acc);color:#fff}
button.s{background:transparent;color:var(--fg)}
#log{margin-top:16px;font:13px ui-monospace,monospace;max-height:50vh;overflow:auto;border-top:1px solid var(--bd);padding-top:8px}
</style></head><body>
<h1>Modo instalación</h1><div class="m">Distancia cruda del sensor, sin filtro ni cálculos (~1 lectura/s)</div>
<div id="d">--</div><div class="m" id="st">conectando…</div>
<button id="t" onclick="tg()">Activar modo instalación</button><button class="s" onclick="cal()">Calibrar fondo con esta distancia</button>
<div id="log"></div>
<script>
let last=-1;
function $(i){return document.getElementById(i)}
async function poll(){try{const j=await (await fetch('/api/sensor-data')).json();
$('t').textContent=j.installationMode?'Salir del modo instalación':'Activar modo instalación';
if(!j.installationMode){$('st').textContent='Modo normal: actívalo para ver la distancia cruda';return}
if(j.readingId===last)return;last=j.readingId;
const t=j.time||new Date().toLocaleTimeString();const v=j.estadoSensor==='Esperando'?'…':(j.estadoSensor==='Sin eco'?'sin eco':Number(j.distancia).toFixed(1)+' cm');
$('d').textContent=v;$('st').textContent='Lectura #'+last+' · '+t;
const l=$('log');l.insertAdjacentHTML('afterbegin','<div>'+t+'  '+v+'</div>');while(l.children.length>60)l.lastChild.remove()}
catch(e){$('st').textContent='sin conexión con el ESP32'}}
async function tg(){await fetch('/api/installation-mode',{method:'POST'});last=-1;poll()}
async function cal(){if(!confirm('¿Guardar la distancia actual como fondo del contenedor vacío?'))return;
const j=await (await fetch('/api/calibrate-bottom',{method:'POST'})).json();alert(j.message||'Listo')}
setInterval(poll,1000);poll();
</script></body></html>)HTML";

// Página /espnow: modo, rol, clave, emparejamiento, unidades y contadores de filtrado.
// En el firmware (no LittleFS) para no tener que subir el filesystem, que borra /config.json
static const char ESPNOW_PAGE_HTML[] PROGMEM = R"HTML(<!doctype html><html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>ESP-NOW</title><style>
:root{--bg:#fcfcfb;--fg:#0b0b0b;--mut:#52514e;--acc:#2a78d6;--bd:#e1e0d9}
@media(prefers-color-scheme:dark){:root{--bg:#1a1a19;--fg:#f0efec;--mut:#c3c2b7;--acc:#3987e5;--bd:#2c2c2a}}
body{margin:0 auto;padding:16px;font-family:system-ui,sans-serif;background:var(--bg);color:var(--fg);max-width:720px}
h1{font-size:18px;margin:0 0 4px}h2{font-size:15px;margin:20px 0 6px}.m{color:var(--mut);font-size:13px}
table{border-collapse:collapse;width:100%;font-size:13px;font-variant-numeric:tabular-nums}
td{border-bottom:1px solid var(--bd);padding:4px 6px;text-align:left;overflow-wrap:anywhere}
label{display:block;margin:10px 0 2px;font-size:13px;color:var(--mut)}
select,input{font-size:15px;padding:6px;border:1px solid var(--bd);border-radius:6px;background:var(--bg);color:var(--fg);width:100%;max-width:420px;box-sizing:border-box}
button{font-size:15px;padding:8px 12px;margin:8px 8px 0 0;border-radius:8px;border:1px solid var(--bd);background:var(--acc);color:#fff}
button.s{background:transparent;color:var(--fg)}button:disabled{opacity:.5}
td button{font-size:13px;padding:4px 8px;margin:0}
#msg{margin-top:8px}
</style></head><body>
<h1>ESP-NOW</h1><div class="m">Enlace directo entre unidades, sin router. Solo se aceptan unidades emparejadas con la misma clave.</div>
<h2>Estado</h2><table id="st"></table>
<h2>Configuración</h2>
<label for="mode">Modo</label><select id="mode">
<option value="apagado">Apagado: nunca se enciende</option>
<option value="automatico">Automático: solo si falta el WiFi 2 min</option>
<option value="manual">Manual: siempre encendido</option></select>
<label for="role">Rol</label><select id="role">
<option value="emisor">Emisor: solo envía su tinaco (la radio puede dormir)</option>
<option value="receptor">Receptor: recibe y muestra los demás (radio despierta, ~0.12 A)</option></select>
<label for="name">Nombre de esta unidad</label><input id="name" maxlength="31">
<label for="key">Clave de emparejamiento (8 a 64 caracteres, la misma en todas)</label>
<input id="key" type="password" maxlength="64" autocomplete="new-password">
<button onclick="save()">Guardar</button>
<div id="msg" class="m"></div>
<h2>Emparejar</h2>
<div class="m">Pulsa Emparejar en las dos unidades con menos de 3 min de diferencia. Mientras dura, la radio se queda despierta.</div>
<button id="pb" onclick="pair()">Emparejar (3 min)</button>
<h2>Unidades emparejadas</h2><table id="peers"></table>
<h2>Tinacos remotos</h2><table id="rem"></table>
<h2>Tramas</h2><table id="cnt"></table>
<div class="m">Pruebas: la otra unidad debe contarlas como repetida o con clave incorrecta. Un emisor con la radio dormida puede no contestar el ping.</div>
<button class="s" onclick="test('ping')">Ping</button><button class="s" onclick="test('replay')">Repetir última trama</button><button class="s" onclick="test('badkey')">Trama con otra clave</button>
<script>
let loaded=false;
const $=i=>document.getElementById(i);
const esc=s=>String(s).replace(/[&<>"']/g,c=>'&#'+c.charCodeAt(0)+';');
function rows(t,r){$(t).innerHTML=r.map(x=>'<tr>'+x.map(c=>'<td>'+c+'</td>').join('')+'</tr>').join('')}
function age(s){return s<0?'nunca':s<90?s+' s':Math.round(s/60)+' min'}
function why(j){if(j.active)return j.pairing?'emparejando':j.fallback?'respaldo sin WiFi':j.mode;
if(j.mode==='apagado')return 'modo Apagado';if(!j.key_set)return 'falta la clave';
if(j.mode==='automatico')return j.peers.length?'con WiFi no hace falta':'sin unidades emparejadas';return ''}
async function poll(){try{const j=await (await fetch('/api/esp-now/status')).json();
if(!loaded){$('mode').value=j.mode;$('role').value=j.role;$('name').value=j.name;loaded=true}
$('key').placeholder=j.key_set?'(guardada; vacío = no cambiar)':'(sin clave ESP-NOW no se enciende)';
const w=why(j);
rows('st',[['ESP-NOW',(j.active?'encendido':'apagado')+(w?' ('+w+')':'')],['Rol',j.role],
['Radio',j.radio_awake?'despierta (recibe)':'modem sleep (ahorro)'],
['WiFi',j.wifi_connected?'conectado':'sin conexión hace '+age(j.wifi_lost_s)+(j.wifi_retry_paced?', reintento cada 60 s':'')],
['Canal',j.channel+' (guardado '+j.saved_channel+')'],['MAC',j.mac_address],
['Emparejamiento',j.pairing?'abierto, quedan '+j.pairing_remaining_s+' s':'cerrado']]);
$('pb').disabled=j.mode==='apagado'||!j.key_set||j.pairing;
rows('peers',j.peers.length?j.peers.map(p=>[esc(p.name||'(sin nombre)'),p.mac,p.role,'visto '+age(p.last_seen_s),
'<button class="s" onclick="del(\''+p.mac+'\')">Quitar</button>']):[['Ninguna']]);
rows('rem',j.remote.length?j.remote.map(r=>[esc(r.name),r.valid?r.percent+' %':'sin eco',r.valid?r.liters+' L':'',
r.valid?r.distance+' cm':'','hace '+age(r.age_s)]):[['Sin lecturas']]);
const s=j.stats;rows('cnt',[['Aceptadas',s.rx_ok],['Clave incorrecta',s.rx_bad_auth],['Repetidas',s.rx_replay],
['MAC no emparejada',s.rx_unknown],['Otro protocolo',s.rx_malformed],['Enviadas OK / fallidas',s.tx_ok+' / '+s.tx_fail]])}
catch(e){rows('st',[['Sin conexión con el ESP32','']])}}
async function post(u,b){const o={method:'POST'};if(b){o.headers={'Content-Type':'application/json'};o.body=JSON.stringify(b)}
const r=await fetch(u,o);let j={};try{j=await r.json()}catch(e){}
$('msg').textContent=j.message||(r.ok?'Listo':'Error '+r.status);return r.ok}
async function save(){const b={mode:$('mode').value,role:$('role').value};if($('name').value)b.sensorName=$('name').value;
const k=$('key').value;if(k){if(k.length<8){$('msg').textContent='La clave debe tener al menos 8 caracteres';return}b.key=k}
if(await post('/api/esp-now/configure',b))$('key').value='';setTimeout(poll,1500)}
async function pair(){await post('/api/esp-now/pair');setTimeout(poll,1500)}
async function del(m){if(!confirm('¿Quitar '+m+'? Habrá que emparejarla de nuevo.'))return;await post('/api/esp-now/disconnect',{mac:m});setTimeout(poll,1500)}
async function test(k){await post('/api/esp-now/test?kind='+k);setTimeout(poll,1500)}
setInterval(poll,2000);poll();
</script></body></html>)HTML";

AppManager::AppManager() {
    Serial.println("AppManager iniciado");
    wifi_connected = false;
    portal_active = false;
    lastDisplayActivity = 0;
    displaySleeping = false;
    _portalStartTime = 0;
    
    // Inicializar IoT integrations como nullptr
    espNowManager = nullptr;
    haMqtt = nullptr;
    ntpSync = nullptr;
    
    // FreeRTOS task para HA MQTT
    _mqttTaskHandle = nullptr;
    _mqttReconfigPending = false;
    _mqttTaskRunning = false;
    _mqttPendingEnabled = false;
}

void AppManager::initialize() {
    Serial.println("=== INICIALIZANDO SISTEMA ===");

    // 🌡️ Bajar CPU de 240 a 80 MHz: WiFi, web server y MQTT funcionan igual
    // y el consumo/calor del ESP32 y del regulador baja notablemente
    setCpuFrequencyMhz(CPU_FREQ_MHZ);
    Serial.printf("⚙️ CPU: %d MHz\n", getCpuFrequencyMhz());

    // �️ Evitar auto-conexión WiFi desde NVS cache del ESP32
    // Sin esto, el ESP32 se conecta automáticamente a la última red
    // incluso si LittleFS fue borrado (las credenciales viven en NVS)
    WiFi.persistent(false);   // No guardar credenciales en NVS automáticamente
    WiFi.enableSTA(false);    // Desactivar STA hasta que decidamos modo
    WiFi.disconnect(true, true); // Desconectar + borrar credenciales NVS
    delay(100);
    
    // �🔍 Diagnosticar razón del último reinicio
    esp_reset_reason_t resetReason = esp_reset_reason();
    Serial.printf("🔍 Razón del reinicio: %d (", resetReason);
    switch(resetReason) {
        case ESP_RST_POWERON:  Serial.print("Encendido"); break;
        case ESP_RST_SW:       Serial.print("Software"); break;
        case ESP_RST_PANIC:    Serial.print("PANIC/Crash"); break;
        case ESP_RST_INT_WDT:  Serial.print("WDT Interrupt"); break;
        case ESP_RST_TASK_WDT: Serial.print("WDT Task"); break;
        case ESP_RST_WDT:      Serial.print("WDT Otro"); break;
        case ESP_RST_DEEPSLEEP: Serial.print("Deep Sleep"); break;
        case ESP_RST_BROWNOUT: Serial.print("Brownout"); break;
        default:               Serial.print("Desconocido"); break;
    }
    Serial.println(")");
    
    // Configurar watchdog para mayor estabilidad
    esp_task_wdt_init(30, true); // 30 segundos timeout, panic habilitado
    esp_task_wdt_add(NULL); // Añadir tarea actual al watchdog
    
    Serial.printf("💾 Memoria libre al inicio: %d bytes\n", ESP.getFreeHeap());
    
    // Inicializar ConfigManager PRIMERO (monta LittleFS internamente)
    if (!config_manager.begin()) {
        Serial.println("⚠️ Error inicializando ConfigManager, usando defaults");
    }
    Serial.println("✅ LittleFS y ConfigManager inicializados");
    
    // ⚡ VERIFICAR DOBLE RESET (DRD usa LittleFS - archivo /drd.dat)
    // NOTA: Requiere que LittleFS esté montado ANTES (config_manager.begin() lo hace)
    
    // Primero: verificar flags de modo forzado
    bool forceNormalBoot = checkForceNormalBoot();
    bool forcePortalBoot = checkForcePortalBoot();
    
    if (forcePortalBoot) {
        Serial.println("📡 FLAG DE PORTAL DETECTADO - Forzando portal cautivo");
        forcePortalMode = true;
        drd.stop();  // Limpiar DRD para evitar doble-trigger
    } else if (forceNormalBoot) {
        Serial.println("🟢 FLAG DE MODO NORMAL DETECTADO - Saltando DRD, intentando WiFi");
        forcePortalMode = false;
        // Limpiar DRD flag también por seguridad
        drd.stop();
    } else if (drd.detectDoubleReset()) {
        Serial.println("🔄 DOBLE RESET DETECTADO - INICIANDO PORTAL CAUTIVO");
        forcePortalMode = true;
    } else {
        Serial.println("✅ Reset normal - Intentando conectar WiFi");
        forcePortalMode = false;
    }
    
    // Verificar si hay redes WiFi guardadas
    NetworkConfig nc = config_manager.getNetworkConfig();
    Serial.printf("📋 Redes WiFi guardadas: %d\n", nc.getWiFiNetworks().size());
    for (const auto& net : nc.getWiFiNetworks()) {
        Serial.printf("   📡 '%s' default=%s\n", net.ssid.c_str(), net.is_default ? "SÍ" : "no");
    }
    
    // Inicializar variables de conexión
    connectionCheckTimer = 0;
    checkingConnection = false;
    
    display_manager.begin();
    
    // Configurar auto-sleep de pantalla (por defecto 30 segundos)
    uint16_t autoSleepTime = config_manager.getAutoSleepTime();
    display_manager.setAutoSleepTime(autoSleepTime);
    Serial.printf("⏰ Auto-sleep configurado: %d segundos\n", autoSleepTime);
    
    // showInitialMessage() ya muestra "Iniciando WiFi..." centrado en el OLED
    
    // Inicializar timer de auto-sleep
    lastDisplayActivity = millis();
    
    esp_task_wdt_reset(); // Reset watchdog
    
    // 🌊 INICIALIZAR SENSOR REAL (CRÍTICO)
    Serial.println("🌊 Inicializando sensores reales...");
    
    // Usar pines hardcodeados para el sensor HC-SR04 (Heltec WiFi Kit 32)
    const uint8_t trigPin = 12;  // Pin trigger del sensor HC-SR04
    const uint8_t echoPin = 13;  // Pin echo del sensor HC-SR04
    
    // Crear y registrar sensor de líquido
    LiquidLevelSensor* liquidSensor = new LiquidLevelSensor(trigPin, echoPin, &config_manager);
    _liquidSensor = liquidSensor;  // Guardar referencia para diagnósticos
    sensor_manager.addSensor(liquidSensor);
    sensor_manager.begin();
    
    Serial.printf("✅ Sensor de líquido inicializado (Trig: %d, Echo: %d)\n", trigPin, echoPin);
    
    esp_task_wdt_reset(); // Reset watchdog antes de red
    initializeNetwork();
    esp_task_wdt_reset(); // Reset watchdog después de red
    
    // Inicializar IoT integrations después de WiFi
    Serial.println("🌐 Inicializando integraciones IoT...");
    
    // NTP Time Sync - PRIMERO para timestamps precisos
    Serial.println("🕐 Configurando sincronización NTP...");
    ntpSync = new NTPTimeSync();
    ntpSync->setTimezone(config_manager.getSensorConfig().getTimezone());
    ntpSync->begin();
    esp_task_wdt_reset(); // Reset watchdog
    
    // ESP-NOW (3.5.0): ya no se inicia siempre. updateEspNow() lo enciende según el modo
    // (Apagado / Manual / Automático), con clave de emparejamiento y unidades emparejadas
    config_manager.getESPNowConfig().initialize();
    espNowManager = ESPNowManager::getInstance();
    _espNowReqMutex = xSemaphoreCreateMutex();
    espNowManager->onPeerUpdated = [this](const uint8_t* mac, const String& name, uint8_t role) {
        persistEspNowPeer(mac, name, role);
    };
    espNowManager->onSensorDataCallback = [this](const SensorData& sd) { onRemoteSensor(sd); };
    applyEspNowConfig();
    {
        const ESPNowConfig& ec = config_manager.getESPNowConfig();
        Serial.printf("🔗 ESP-NOW: modo %s, rol %s, clave %s, %u unidades, canal guardado %u\n",
                      ESPNowConfig::modeToString(ec.getMode()), ESPNowConfig::roleToString(ec.getRole()),
                      ec.hasPairingKey() ? "sí" : "no", (unsigned)ec.getPeers().size(), ec.getLastChannel());
    }
    // Intento de reconexión espaciado: saber cuándo terminó (sin AP, clave mala...)
    WiFi.onEvent([this](arduino_event_id_t event, arduino_event_info_t info) {
        if (_wifiRetryInProgress && info.wifi_sta_disconnected.reason != WIFI_REASON_ASSOC_LEAVE) {
            _wifiRetryEnded = true;
        }
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    if (!portal_active) updateEspNow();
    
    // Home Assistant MQTT (local broker - Auto-Discovery)
    haMqtt = new HomeAssistantMQTT();
    HomeAssistantMQTT::Config mqttConfig;
    
    // Cargar configuración MQTT desde ConfigManager
    MQTTConfig& savedMqtt = config_manager.getMQTTConfig();
    if (savedMqtt.isConfigured()) {
        mqttConfig.brokerHost    = savedMqtt.getBrokerHost();
        mqttConfig.brokerPort    = savedMqtt.getBrokerPort();
        mqttConfig.username      = savedMqtt.getUsername();
        mqttConfig.password      = savedMqtt.getPassword();
        mqttConfig.deviceName    = savedMqtt.getDeviceName();
        Serial.println("🏠 HA MQTT: Config cargada desde ConfigManager");
    } else {
        Serial.println("🏠 HA MQTT: Sin configurar (configura vía dashboard)");
    }
    mqttConfig.reportInterval = 30000;
    haMqtt->setConfig(mqttConfig);
    haMqtt->setEnabled(savedMqtt.isEnabled());
    haMqtt->setCommandCallback([this](const String& topic, const String& payload) {
        handleHACommand(topic, payload);
    });
    if (haMqtt->isEnabled() && haMqtt->isConfigured()) {
        Serial.println("🏠 HA MQTT habilitado - conectará tras WiFi");
    } else {
        Serial.println("⏸️ HA MQTT deshabilitado");
    }
    
    // Crear FreeRTOS task para HA MQTT (Core 0, no bloquea loop principal)
    if (haMqtt && haMqtt->isEnabled() && haMqtt->isConfigured() && wifi_connected && !_mqttTaskRunning) {
        xTaskCreatePinnedToCore(
            mqttTaskFunc,       // Task function
            "HA_MQTT",          // Name
            8192,               // Stack size (sin TLS, 8KB es suficiente)
            this,               // Parameter
            0,                  // Priority 0
            &_mqttTaskHandle,   // Task handle
            0                   // Core 0
        );
        Serial.println("🏠 HA MQTT: Task creado en Core 0 (no-bloqueante)");
    }
    
    // NOTA: No llamar drd.stop() aquí - dejar que drd.loop() limpie el flag
    // después del timeout (10s). Esto garantiza que el usuario tiene toda
    // la ventana de 10 segundos para hacer doble reset, independientemente
    // de cuánto tarde la inicialización.
    
    Serial.println("🚀 Sistema inicializado COMPLETO con IoT");
    Serial.printf("💾 Memoria libre tras inicialización: %d bytes\n", ESP.getFreeHeap());
}

void AppManager::initializeNetwork() {
    Serial.printf("🌐 initializeNetwork() - forcePortalMode=%s\n", forcePortalMode ? "true" : "false");
    
    if (forcePortalMode) {
        // 🔄 MODO PORTAL CAUTIVO (Doble Reset)
        Serial.println("🔄 INICIANDO PORTAL CAUTIVO...");
        startPortalMode();
    } else {
        // ✅ MODO NORMAL - Intentar conectar WiFi
        Serial.println("🌐 INTENTANDO CONECTAR WIFI PREDETERMINADO...");
        if (tryConnectWiFi()) {
            startNormalMode();
        } else {
            Serial.println("❌ No se pudo conectar - Iniciando portal cautivo");
            startPortalMode();
        }
    }
}

void AppManager::startPortalMode() {
    // Limpiar estado WiFi completamente antes de configurar AP_STA
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(150);
    
    // Usar AP_STA para que el escaneo WiFi funcione correctamente en modo portal
    WiFi.mode(WIFI_AP_STA);
    delay(100);
    WiFi.softAPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
    
    String ap_ssid = config_manager.getAPSSID();
    String ap_password = config_manager.getAPPassword();
    
    bool ap_result = WiFi.softAP(ap_ssid.c_str(), ap_password.c_str());
    
    if (ap_result) {
        Serial.println("PORTAL CAUTIVO ACTIVO");
        apCallback();
        
        dnsServer.start(53, "*", WiFi.softAPIP());
        
        setupWebServer();
        wifi_connected = false;
        portal_active = true;
        _portalStartTime = millis();
        Serial.printf("Portal: http://%s\n", WiFi.softAPIP().toString().c_str());
        
        // ENABLE AUTO-SLEEP 5 MINUTES IN PORTAL MODE
        // PRG button wakes display automatically
        display_manager.enableAutoSleep(true);
        display_manager.setAutoSleepTime(300);  // 5 minutes
        lastDisplayActivity = millis();  // Reset inactivity counter
        
        // Show portal info on OLED
        display_manager.clear();
        display_manager.setFont(ArialMT_Plain_10);
        display_manager.setTextAlignment(TEXT_ALIGN_CENTER);
        display_manager.drawString(64, 0, "PORTAL CAUTIVO");
        display_manager.setTextAlignment(TEXT_ALIGN_LEFT);
        display_manager.drawString(0, 14, "WiFi: " + ap_ssid);
        if (ap_password.length() > 0) {
            display_manager.drawString(0, 27, "Pass: " + ap_password);
        } else {
            display_manager.drawString(0, 27, "Pass: (sin clave)");
        }
        display_manager.drawString(0, 40, "IP: 192.168.1.1");
        display_manager.setTextAlignment(TEXT_ALIGN_CENTER);
        display_manager.drawString(64, 53, "Conectate al WiFi");
        display_manager.display();
        
        Serial.println("OLED will sleep in 5 minutes. Press PRG to wake.");
    } else {
        Serial.println("ERROR creando AP");
    }
}

bool AppManager::tryConnectWiFi() {
    NetworkConfig network_config = config_manager.getNetworkConfig();
    const auto& networks = network_config.getWiFiNetworks();
    
    Serial.printf("Buscando red WiFi entre %d redes guardadas\n", networks.size());
    
    if (networks.empty()) {
        Serial.println("No hay redes WiFi guardadas");
        // Mostrar en OLED que no hay redes
        display_manager.clear();
        display_manager.setFont(ArialMT_Plain_10);
        display_manager.setTextAlignment(TEXT_ALIGN_CENTER);
        display_manager.drawString(64, 15, "Sin redes WiFi");
        display_manager.drawString(64, 30, "guardadas");
        display_manager.display();
        delay(1500);
        return false;
    }
    
    // Listar redes para diagnóstico
    for (const auto& network : networks) {
        Serial.printf("   Red: '%s' default=%s\n", network.ssid.c_str(), network.is_default ? "SI" : "no");
    }
    
    // Paso 1: Intentar la red predeterminada primero
    for (const auto& network : networks) {
        if (network.is_default && network.ssid.length() > 0) {
            Serial.printf("Intentando red predeterminada: '%s'\n", network.ssid.c_str());
            if (tryConnectToNetwork(network)) {
                return true;
            }
            Serial.println("Red predeterminada fallo, intentando alternativas...");
            break;
        }
    }
    
    // Paso 2: Intentar redes no-default
    for (const auto& network : networks) {
        if (!network.is_default && network.ssid.length() > 0) {
            if (tryConnectToNetwork(network)) {
                return true;
            }
        }
    }
    
    Serial.println("No se pudo conectar a ninguna red WiFi guardada");
    return false;
}

void AppManager::startNormalMode() {
    wifi_connected = true;
    portal_active = false;
    
    Serial.println("🌊 INICIANDO MODO SENSOR NORMAL");
    
    // Desactivar el SoftAP para que no siga publicando la red del portal
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    Serial.println("📡 SoftAP desactivado - Solo modo estación WiFi");
    // Modem sleep activo: la radio dormía nunca (~100mA continuos = calor).
    // Solo se desactiva para recibir por ESP-NOW (ver updateWiFiPowerSave)
    updateWiFiPowerSave(true);
    
    // Iniciar mDNS para resolución por nombre (http://hostname.local)
    String hostname = WiFi.getHostname();
    if (MDNS.begin(hostname.c_str())) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("📛 mDNS iniciado: http://%s.local\n", hostname.c_str());
    } else {
        Serial.println("⚠️ Error iniciando mDNS");
    }
    
    setupWebServer(); // Web server unificado inteligente
    
    // Mostrar info en pantalla OLED
    display_manager.clear();
    display_manager.setFont(ArialMT_Plain_10);
    display_manager.setTextAlignment(TEXT_ALIGN_LEFT);
    display_manager.drawString(0, 0, "SENSOR ACTIVO");
    display_manager.drawString(0, 12, "WiFi: " + WiFi.SSID());
    display_manager.drawString(0, 24, "IP: " + WiFi.localIP().toString());
    display_manager.drawString(0, 36, "Leyendo sensor...");
    display_manager.display();
    
    // Habilitar auto-sleep
    display_manager.enableAutoSleep(true);
    
    // HA MQTT task se crea en initialize() después de crear el objeto haMqtt
}

// Modem sleep (WIFI_PS_MIN_MODEM): la radio duerme entre beacons del router.
// Baja el consumo de ~100mA a ~25-40mA a cambio de ~100-300ms de latencia de ping.
// ESP-NOW puede enviar en modem sleep pero no recibir: solo se desactiva en el
// receptor con ESP-NOW activo y durante la ventana de emparejamiento.
void AppManager::updateWiFiPowerSave(bool force) {
    bool mustReceive = espNowManager && espNowManager->isInit() &&
                       (espNowManager->isPairing() || _espNowPairingRequested ||
                        config_manager.getESPNowConfig().isReceptor());
    bool wantPowerSave = !mustReceive;
    if (!force && wantPowerSave == _wifiPowerSave) return;
    WiFi.setSleep(wantPowerSave);
    _wifiPowerSave = wantPowerSave;
    Serial.printf("📶 WiFi modem sleep: %s\n", wantPowerSave ? "ON (ahorro)" : "OFF (ESP-NOW recibiendo)");
}

// ===== ESP-NOW: configuración y máquina de estados =====

void AppManager::queueEspNowRequest(const std::function<void(EspNowRequest&)>& fill) {
    if (!_espNowReqMutex) return;
    xSemaphoreTake(_espNowReqMutex, portMAX_DELAY);
    fill(_espNowReq);
    xSemaphoreGive(_espNowReqMutex);
}

// Aplica en el loop los cambios pedidos por la web (modo, rol, clave, nombre, unidades)
void AppManager::processEspNowRequests() {
    if (!_espNowReqMutex) return;
    EspNowRequest req;
    xSemaphoreTake(_espNowReqMutex, portMAX_DELAY);
    req = _espNowReq;
    _espNowReq = EspNowRequest();
    xSemaphoreGive(_espNowReqMutex);
    if (!req.setMode && !req.setRole && !req.setKey && !req.setName && !req.addPeer && !req.removePeer) return;

    ESPNowConfig& cfg = config_manager.getESPNowConfig();
    config_manager.lock();
    if (req.setMode) cfg.setMode((EspNowMode)req.mode);
    if (req.setRole) cfg.setRole((EspNowRole)req.role);
    if (req.setKey) cfg.setPairingKey(req.key);
    if (req.setName) cfg.setCustomSensorName(req.name);
    if (req.addPeer) {
        bool exists = false;
        for (const auto& p : cfg.getPeers()) exists |= memcmp(p.mac, req.addMac, 6) == 0;
        bool changed;
        if (!exists) cfg.upsertPeer(req.addMac, "", ESPNOW_ROLE_EMISOR, changed);  // Nombre y rol llegan en el HELLO
    }
    if (req.removePeer) {
        cfg.removePeer(req.removeMac);
        espNowManager->removePeer(req.removeMac);
    }
    config_manager.save();
    config_manager.unlock();
    applyEspNowConfig();
    Serial.printf("⚙️ ESP-NOW: modo %s, rol %s, clave %s, %u unidades\n",
                  ESPNowConfig::modeToString(cfg.getMode()), ESPNowConfig::roleToString(cfg.getRole()),
                  cfg.hasPairingKey() ? "sí" : "no", (unsigned)cfg.getPeers().size());
    if (!display_manager.isDisplaySleeping() && !_installationMode) updateSensorDisplay(true);
}

void AppManager::applyEspNowConfig() {
    if (!espNowManager) return;
    const ESPNowConfig& cfg = config_manager.getESPNowConfig();
    espNowManager->setKey(cfg.hasPairingKey() ? cfg.getPairingKey() : String());
    espNowManager->setIdentity(cfg.getSensorID(), cfg.getSensorName(), (uint8_t)cfg.getRole());
    std::vector<ESPNowManager::PeerInfo> list;
    for (const auto& p : cfg.getPeers()) {
        ESPNowManager::PeerInfo pi;
        memcpy(pi.mac, p.mac, 6);
        pi.name = p.name;
        pi.role = (uint8_t)p.role;
        list.push_back(pi);
    }
    espNowManager->setPairedPeers(list);
}

// Unidad nueva o con nombre/rol distinto (llega por HELLO con clave válida)
void AppManager::persistEspNowPeer(const uint8_t* mac, const String& name, uint8_t role) {
    ESPNowConfig& cfg = config_manager.getESPNowConfig();
    config_manager.lock();
    bool changed = false;
    bool ok = cfg.upsertPeer(mac, name, (EspNowRole)role, changed);
    if (ok && changed) config_manager.save();
    config_manager.unlock();
    if (!ok) Serial.println("⚠️ ESP-NOW: no caben más unidades emparejadas");
}

static void setRadioChannel(uint8_t ch) {
    if (ch < 1 || ch > 13) return;
    uint8_t cur;
    wifi_second_chan_t sec;
    if (esp_wifi_get_channel(&cur, &sec) == ESP_OK && cur == ch) return;
    if (esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE) == ESP_OK) {
        Serial.printf("📻 Radio en canal %u (último del router)\n", ch);
    }
}

void AppManager::finishWifiRetry() {
    _wifiRetryInProgress = false;
    _wifiRetryEnded = false;
    _lastWifiRetry = millis();
    if (WiFi.status() == WL_CONNECTED) return;
    WiFi.disconnect(false, false);  // Detiene el intento; conserva SSID/clave
    setRadioChannel(config_manager.getESPNowConfig().getLastChannel());
}

// paced = ESP-NOW activo y sin WiFi desde hace ESPNOW_FALLBACK_MS
void AppManager::updateWiFiReconnect(bool wifiUp, bool paced) {
    // Caída simulada: sin reintentos hasta que venza; luego se reconecta como si volviera el router
    if (_wifiSimActive && (long)(millis() - _wifiSimUntil) >= 0) {
        _wifiSimActive = false;
        Serial.println("🧪 Fin de la caída de WiFi simulada: reconectando");
        if (_wifiPacedReconnect) {
            _lastWifiRetry = millis() - WIFI_RETRY_PERIOD_MS;  // Intento inmediato
        } else {
            WiFi.setAutoReconnect(true);
            WiFi.begin();
        }
    }
    if (wifiUp) {
        if (_wifiPacedReconnect) {
            _wifiPacedReconnect = false;
            _wifiRetryInProgress = false;
            WiFi.setAutoReconnect(true);
            Serial.println("📶 WiFi recuperado: reconexión automática normal");
        }
        return;
    }
    if (!paced) {
        if (_wifiPacedReconnect) {
            // Ya no hace falta canal fijo (p. ej. modo Apagado): volver al auto-reconnect continuo
            _wifiPacedReconnect = false;
            _wifiRetryInProgress = false;
            if (!_wifiSimActive) {
                WiFi.setAutoReconnect(true);
                WiFi.begin();
            }
        }
        return;
    }
    uint8_t ch = config_manager.getESPNowConfig().getLastChannel();
    if (!_wifiPacedReconnect) {
        _wifiPacedReconnect = true;
        WiFi.setAutoReconnect(false);
        WiFi.disconnect(false, false);
        _wifiRetryInProgress = false;
        _lastWifiRetry = millis();
        setRadioChannel(ch);
        Serial.printf("📶 Sin WiFi: ESP-NOW en canal %u, reintento del router cada %lus\n",
                      ch, WIFI_RETRY_PERIOD_MS / 1000);
        return;
    }
    if (_wifiRetryInProgress) return;  // finishWifiRetry() lo cierra (evento o timeout)
    if (!_wifiSimActive && millis() - _lastWifiRetry >= WIFI_RETRY_PERIOD_MS) {
        // El escaneo rápido empieza por el último canal: si el router volvió ahí, conecta enseguida
        wifi_config_t conf;
        if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
            conf.sta.channel = ch;
            esp_wifi_set_config(WIFI_IF_STA, &conf);
        }
        _wifiRetryEnded = false;
        _wifiRetryInProgress = true;
        _wifiRetryStart = millis();
        if (esp_wifi_connect() != ESP_OK) finishWifiRetry();
    } else {
        setRadioChannel(ch);  // Por si un escaneo previo dejó la radio en otro canal
    }
}

// Junta el cuerpo de un POST: el navegador puede mandarlo en varios segmentos TCP y el
// handler de cuerpo se llama una vez por segmento (parsear solo el primero daba "JSON invalido").
// 1 = completo (texto en request->_tempObject, que la librería libera con free), 0 = faltan trozos, -1 = error
static int collectBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    if (total == 0 || total > 1024) return -1;
    if (index == 0) {
        free(request->_tempObject);
        request->_tempObject = malloc(total + 1);
    }
    char* buf = (char*)request->_tempObject;
    if (!buf || index + len > total) return -1;
    memcpy(buf + index, data, len);
    if (index + len < total) return 0;
    buf[total] = '\0';
    return 1;
}

static bool espNowModeUsable(const ESPNowConfig& cfg) {
    return cfg.getMode() != ESPNOW_MODE_OFF && cfg.hasPairingKey();
}

void AppManager::updateEspNow() {
    if (!espNowManager) return;
    processEspNowRequests();
    if (_wifiSimRequestMs) {
        _wifiSimUntil = millis() + _wifiSimRequestMs;
        _wifiSimRequestMs = 0;
        _wifiSimActive = true;
        WiFi.setAutoReconnect(false);
        WiFi.disconnect(false, false);
        Serial.printf("🧪 Caída de WiFi simulada: %lus sin router\n", (_wifiSimUntil - millis()) / 1000);
    }
    ESPNowConfig& cfg = config_manager.getESPNowConfig();
    bool wifiUp = WiFi.status() == WL_CONNECTED;

    if (wifiUp) {
        if (_wifiLostSince) {
            Serial.printf("📶 WiFi de vuelta tras %lus\n", (millis() - _wifiLostSince) / 1000);
            _wifiLostSince = 0;
        }
        // Canal del router para el respaldo sin WiFi (se guarda solo si cambia)
        uint8_t ch = WiFi.channel();
        if (ch >= 1 && ch <= 13 && ch != cfg.getLastChannel()) {
            config_manager.lock();
            cfg.setLastChannel(ch);
            config_manager.save();
            config_manager.unlock();
            Serial.printf("📻 Canal del router guardado: %u\n", ch);
        }
    } else if (_wifiLostSince == 0) {
        _wifiLostSince = millis() | 1;
        Serial.println("📶 WiFi perdido");
    }
    bool lostLong = !wifiUp && millis() - _wifiLostSince >= ESPNOW_FALLBACK_MS;

    if (_espNowResetRequested) {
        _espNowResetRequested = false;
        espNowManager->deinitESPNow();  // Se vuelve a encender abajo si corresponde
    }

    bool usable = espNowModeUsable(cfg);
    if (_espNowPairingRequested && !usable) {
        _espNowPairingRequested = false;
        Serial.println("⚠️ Emparejamiento ignorado: modo Apagado o sin clave");
    }
    bool want = false;
    if (usable) {
        if (cfg.getMode() == ESPNOW_MODE_MANUAL) want = true;
        else want = cfg.hasPeers() && lostLong;  // Automático
        if (_espNowPairingRequested || espNowManager->isPairing()) want = true;
    }
    _espNowFallback = cfg.getMode() == ESPNOW_MODE_AUTO && want && lostLong;

    updateWiFiReconnect(wifiUp, want && lostLong);

    bool wasInit = espNowManager->isInit();
    if (want && !wasInit) {
        espNowManager->initESPNow();
    } else if (!want && wasInit) {
        espNowManager->deinitESPNow();
    }
    if (_espNowPairingRequested && espNowManager->isInit()) {
        _espNowPairingRequested = false;
        espNowManager->startPairing(ESPNOW_PAIRING_MS);
    }
    if (wasInit != espNowManager->isInit()) {
        Serial.printf("🔗 ESP-NOW %s (%s)\n", espNowManager->isInit() ? "ENCENDIDO" : "APAGADO",
                      _espNowFallback ? "respaldo sin WiFi" : ESPNowConfig::modeToString(cfg.getMode()));
        if (!display_manager.isDisplaySleeping() && !_installationMode) updateSensorDisplay(true);
    }
    updateWiFiPowerSave();
}

// Lectura de una unidad emparejada (desde ESPNowManager::loop, contexto del loop principal)
void AppManager::onRemoteSensor(const SensorData& sd) {
    if (!config_manager.getESPNowConfig().isReceptor()) return;
    Serial.printf("📥 ESP-NOW %s: %.1f%% %.1fL %.1fcm%s\n", sd.sensorName, sd.liquidLevel, sd.liters,
                  sd.distance, (sd.flags & ESPNOW_FLAG_VALID) ? "" : " (sin eco)");
    // Modo Manual con WiFi: el receptor reenvía el tinaco remoto a Home Assistant
    if (haMqtt && haMqtt->isConnected() && (sd.flags & ESPNOW_FLAG_VALID)) {
        haMqtt->publishRemoteSensor(sd.mac, sd.sensorName, sd.liters, sd.liquidLevel, sd.distance);
    }
}

// Receptor: la línea inferior del OLED alterna cada 4 s entre el estado local y cada tinaco remoto
String AppManager::currentRemoteLine() {
    if (!espNowManager || !espNowManager->isInit() || !config_manager.getESPNowConfig().isReceptor()) return "";
    std::vector<SensorData> fresh;
    for (const auto& s : espNowManager->getConnectedSensors()) {
        if (millis() - s.timestamp < ESPNOW_REMOTE_FRESH_MS) fresh.push_back(s);
    }
    if (fresh.empty()) return "";
    size_t slot = (millis() / 4000) % (fresh.size() + 1);
    if (slot == 0) return "";
    const SensorData& s = fresh[slot - 1];
    String name = String(s.sensorName).substring(0, 10);
    if (!(s.flags & ESPNOW_FLAG_VALID)) return name + ": sin eco";
    return name + " " + String((int)s.liquidLevel) + "% " + String(s.liters, 0) + "L";
}

String AppManager::espNowStatusJson() {
    const ESPNowConfig& cfg = config_manager.getESPNowConfig();
    DynamicJsonDocument doc(3072);
    bool init = espNowManager && espNowManager->isInit();
    doc["esp_now_enabled"] = init;
    doc["active"] = init;
    doc["mode"] = ESPNowConfig::modeToString(cfg.getMode());
    doc["role"] = ESPNowConfig::roleToString(cfg.getRole());
    doc["name"] = espNowManager ? espNowManager->getLocalName() : String("");
    doc["key_set"] = cfg.hasPairingKey();   // La clave nunca se devuelve
    doc["fallback"] = _espNowFallback;
    doc["radio_awake"] = !_wifiPowerSave;
    doc["pairing"] = espNowManager && (espNowManager->isPairing() || _espNowPairingRequested);
    doc["pairing_remaining_s"] = espNowManager ? espNowManager->pairingRemainingMs() / 1000 : 0;
    doc["mac_address"] = WiFi.macAddress();
    doc["channel"] = WiFi.channel();
    doc["saved_channel"] = cfg.getLastChannel();
    doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
    doc["wifi_lost_s"] = _wifiLostSince ? (millis() - _wifiLostSince) / 1000 : 0;
    doc["wifi_retry_paced"] = _wifiPacedReconnect;
    if (_wifiSimActive) doc["wifi_sim_s"] = (long)(_wifiSimUntil - millis()) / 1000;
    doc["wifi_mode"] = WiFi.getMode() == WIFI_AP_STA ? "AP_STA" : "STA";
    doc["connected_peers"] = espNowManager ? espNowManager->getConnectedSensorCount() : 0;
    doc["packets_sent"] = espNowManager ? espNowManager->getPacketsSent() : 0;
    doc["packets_received"] = espNowManager ? espNowManager->getPacketsReceived() : 0;
    doc["last_activity"] = millis();
    if (espNowManager) {
        espNowManager->peersToJson(doc.createNestedArray("peers"));
        espNowManager->sensorsToJson(doc.createNestedArray("remote"));
        espNowManager->statsToJson(doc.createNestedObject("stats"));
    }
    String json;
    serializeJson(doc, json);
    return json;
}

// Entrar/salir del modo instalación (dashboard, WebSerial o /instalacion)
void AppManager::setInstallationMode(bool on) {
    if (on == _installationMode) return;
    if (on) {
        _liveMonitorEnabled = false;
        _installRawCm = INSTALL_WAITING;  // Aún sin lectura cruda (no confundir con "sin eco")
        _installationMode = true;
        Serial.println("🔧 Modo instalacion ACTIVADO: distancia cruda ~1/s");
        return;
    }
    _installationMode = false;
    // El sensor se movió: limpiar el filtro (historial, spike guard, failsafe) partiendo de la
    // última distancia cruda, y leer de inmediato en vez de esperar el intervalo normal
    if (_liquidSensor) _liquidSensor->resetFilterState(_installRawCm);
    _resetFlowHistory = true;
    _forceSensorRead = true;
    updateSensorDisplay(true);
    display_manager.enableAutoSleep(true);
    Serial.println("✅ Modo instalacion DESACTIVADO: filtro reiniciado, lectura inmediata");
}

// Distancia para calibrar el fondo: la cruda en modo instalación, la filtrada si no
float AppManager::calibrationDistance() {
    if (_installationMode) return _installRawCm;
    return (_liquidSensor && _liquidSensor->hasValidReading()) ? _liquidSensor->getDistance() : -1.0f;
}

// ============================================================
// FreeRTOS Task para HA MQTT (Core 0, no bloquea el loop principal)
// ============================================================
void AppManager::mqttTaskFunc(void* param) {
    AppManager* app = static_cast<AppManager*>(param);
    app->_mqttTaskRunning = true;
    
    // Esperar 5 segundos tras boot para estabilizar WiFi y web server
    Serial.println("🏠 [MQTTTask] Esperando 5s para estabilizar sistema...");
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Intentar conexión inicial
    if (app->haMqtt && app->haMqtt->isEnabled() && app->wifi_connected) {
        Serial.printf("🏠 [MQTTTask] Iniciando conexión (heap: %u bytes)\n", ESP.getFreeHeap());
        app->haMqtt->begin();
    }
    
    // Loop infinito del task
    while (true) {
        // Verificar si hay reconfiguración pendiente (desde POST /api/mqtt/config)
        if (app->_mqttReconfigPending) {
            Serial.println("🏠 [MQTTTask] Aplicando nueva configuración...");
            if (app->haMqtt) {
                app->haMqtt->disconnect();
                app->haMqtt->setConfig(app->_mqttPendingConfig);
                app->haMqtt->setEnabled(app->_mqttPendingEnabled);
                
                if (app->_mqttPendingEnabled && app->wifi_connected) {
                    app->haMqtt->begin();
                }
            }
            app->_mqttReconfigPending = false;
        }
        
        // Loop normal de HA MQTT (reconexión, keepalive)
        if (app->haMqtt && app->wifi_connected && app->haMqtt->isEnabled()) {
            app->haMqtt->loop();
        }
        
        // Ceder CPU — 100ms entre iteraciones
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void AppManager::setupWebServer() {
    Serial.println("🌐 Configurando Web Server Unificado Inteligente...");
    
    // ===== ARCHIVOS ESTÁTICOS COMPARTIDOS =====
    setupStaticFiles();
    
    // ===== RUTAS PRINCIPALES INTELIGENTES =====
    setupSmartRoutes();
    
    // ===== RUTAS CONDICIONALES POR MODO =====
    setupConditionalRoutes();
    
    // ===== RUTAS DE DETECCIÓN DE PORTAL CAUTIVO =====
    setupCaptiveDetectionRoutes();
    
    // ===== SERVICIOS ESPECÍFICOS POR MODO =====
    initializeServices();
    
    // ===== INICIALIZAR SERVIDOR =====
    server.begin();
    Serial.printf("✅ Web Server iniciado en modo: %s\n", 
                  portal_active ? "PORTAL CAUTIVO" : "SENSOR NORMAL");
}

void AppManager::setupStaticFiles() {
    // Archivos estáticos: el minificador genera archivos con el mismo nombre
    // en data/web/{modulo}/ desde data/web/{modulo}/src/
    // Estructura en LittleFS: /web/{modulo}/{archivo}.{ext}
    // NOTA: Los HTML usan rutas absolutas para evitar conflictos
    //       (ej: /web/captive_portal/style.css en vez de style.css relativo)
    
    // === CAPTIVE PORTAL ===
    server.serveStatic("/web/captive_portal/style.css", LittleFS, "/web/captive_portal/style.css").setCacheControl("max-age=86400");
    server.serveStatic("/web/captive_portal/captive-portal.js", LittleFS, "/web/captive_portal/captive-portal.js").setCacheControl("max-age=86400");
    
    // === ESP-NOW Manager ===
    server.serveStatic("/web/esp_now/style.css", LittleFS, "/web/esp_now/style.css").setCacheControl("max-age=86400");
    server.serveStatic("/web/esp_now/esp-now-manager.js", LittleFS, "/web/esp_now/esp-now-manager.js").setCacheControl("max-age=86400");
    
    // === Dashboard ===
    server.serveStatic("/web/dashboard/style.css", LittleFS, "/web/dashboard/style.css").setCacheControl("max-age=86400");
    server.serveStatic("/web/dashboard/dashboard.js", LittleFS, "/web/dashboard/dashboard.js").setCacheControl("max-age=86400");
    server.serveStatic("/web/dashboard/assets/", LittleFS, "/web/dashboard/assets/").setCacheControl("max-age=86400");
    
    // === ESP-NOW Dashboard ===
    server.serveStatic("/web/espnow_dashboard/style.css", LittleFS, "/web/espnow_dashboard/style.css").setCacheControl("max-age=86400");
    server.serveStatic("/web/espnow_dashboard/espnow-dashboard.js", LittleFS, "/web/espnow_dashboard/espnow-dashboard.js").setCacheControl("max-age=86400");
    
    // === Assets y compartidos ===
    server.serveStatic("/web/captive_portal/assets/", LittleFS, "/web/captive_portal/assets/").setCacheControl("max-age=86400");
    server.serveStatic("/web/esp_now/assets/", LittleFS, "/web/esp_now/assets/").setCacheControl("max-age=86400");
    server.serveStatic("/web/shared/", LittleFS, "/web/shared/").setCacheControl("max-age=86400");
    
    // Imágenes del sensor (disponibles en ambos modos)
    server.serveStatic("/imagen_vacio.jpg", LittleFS, "/imagen_vacio.jpg", "image/jpeg");
    server.serveStatic("/imagen_lleno.jpg", LittleFS, "/imagen_lleno.jpg", "image/jpeg");
    server.serveStatic("/imagen_error.jpg", LittleFS, "/imagen_error.jpg", "image/jpeg");
}

// ===========================================================================
// WEB SERVER UNIFICADO INTELIGENTE - FUNCIONES ESPECIALIZADAS
// ===========================================================================

void AppManager::setupSmartRoutes() {
    Serial.println("🧠 Configurando rutas inteligentes...");
    
    // ===== RUTA PRINCIPAL INTELIGENTE "/" =====
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        Serial.printf("📄 Petición ruta / - Heap: %d bytes\n", ESP.getFreeHeap());
        
        if (portal_active) {
            // MODO PORTAL CAUTIVO
            if (LittleFS.exists("/web/captive_portal/index.html")) {
                request->send(LittleFS, "/web/captive_portal/index.html", "text/html");
            } else {
                request->send(500, "text/plain", "Error: Portal cautivo no encontrado");
            }
        } else {
            // MODO SENSOR NORMAL
            if (LittleFS.exists("/web/dashboard/index.html")) {
                request->send(LittleFS, "/web/dashboard/index.html", "text/html");
            } else {
                request->send(500, "text/plain", "Error: dashboard/index.html no encontrado");
            }
        }
    });
    
    // ===== PÁGINA DE INSTALACIÓN (embebida en el firmware) =====
    // Distancia cruda ~1 lectura/s para posicionar el sensor. Va en el firmware para no
    // tener que subir el filesystem (eso borraría WiFi/MQTT de /config.json)
    server.on("/instalacion", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", INSTALL_PAGE_HTML);
    });

    // ===== PÁGINAS ESP-NOW =====
    // Sus index.html nunca tuvieron ruta: el botón multi-sensor del dashboard y los
    // enlaces de regreso daban 404
    server.on("/espnow_dashboard", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/web/espnow_dashboard/index.html", "text/html");
    });
    // Configuración ESP-NOW (modo, rol, clave, emparejamiento): embebida en el firmware.
    // La página LittleFS anterior no conocía la clave ni los modos
    server.on("/espnow", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", ESPNOW_PAGE_HTML);
    });
    server.on("/espnow-manager", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/espnow");
    });
    // Enlaces de regreso usados por las páginas ESP-NOW
    server.on("/dashboard", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    server.on("/dashboard/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });

    // ===== API STATUS INTELIGENTE =====
    server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        String json;
        if (portal_active) {
            json = "{\"mode\":\"portal\",\"wifi_connected\":false,\"ap_active\":true,\"ap_ip\":\"" + 
                   WiFi.softAPIP().toString() + "\",\"status\":\"" +
                   String(display_manager.isDisplaySleeping() ? "Apagado" : "Encendido") + "\"}";
        } else {
            // Diagnóstico térmico/energía: temp_c es el sensor interno del ESP32
            // (absoluto impreciso, útil para comparar antes/después)
            json = "{\"mode\":\"sensor\",\"wifi_connected\":true,\"ip\":\"" +
                   WiFi.localIP().toString() + "\",\"ssid\":\"" + WiFi.SSID() +
                   "\",\"status\":\"" +
                   String(display_manager.isDisplaySleeping() ? "Apagado" : "Encendido") + "\"" +
                   ",\"version\":\"" + String(FIRMWARE_VERSION) + "\"" +
                   ",\"temp_c\":" + String(temperatureRead(), 1) +
                   ",\"cpu_mhz\":" + String(getCpuFrequencyMhz()) +
                   ",\"wifi_sleep\":" + String(_wifiPowerSave ? "true" : "false") +
                   ",\"espnow_mode\":\"" + String(ESPNowConfig::modeToString(config_manager.getESPNowConfig().getMode())) + "\"" +
                   ",\"espnow_active\":" + String((espNowManager && espNowManager->isInit()) ? "true" : "false") +
                   ",\"espnow_pairing\":" + String((espNowManager && espNowManager->isPairing()) ? "true" : "false") +
                   ",\"espnow_peers\":" + String((espNowManager && espNowManager->hasPeers()) ? "true" : "false") +
                   ",\"rssi\":" + String(WiFi.RSSI()) +
                   ",\"uptime_s\":" + String(millis() / 1000) +
                   ",\"heap\":" + String(ESP.getFreeHeap()) +
                   ",\"reset_reason\":" + String((int)esp_reset_reason()) + "}";
        }
        request->send(200, "application/json", json);
    });
}

void AppManager::setupConditionalRoutes() {
    Serial.println("🔀 Configurando rutas condicionales...");
    
    // ===== RUTAS EXCLUSIVAS DEL PORTAL CAUTIVO =====
    server.on("/scan-wifi", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!portal_active) {
            request->send(404, "text/plain", "❌ Scan WiFi solo disponible en modo portal");
            return;
        }
        handleScanWiFi(request);
    });
    
    server.on("/wifi-results", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!portal_active) {
            request->send(404, "text/plain", "❌ WiFi results solo disponible en modo portal");
            return;
        }
        handleWiFiResults(request);
    });
    
    // Endpoint para agregar red WiFi
    server.on("/api/wifi/add", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL, 
    [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (!portal_active) {
            request->send(404, "application/json", "{\"success\":false,\"message\":\"Solo disponible en modo portal\"}");
            return;
        }
        
        String body = "";
        for (size_t i = 0; i < len; i++) {
            body += (char)data[i];
        }
        
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, body);
        if (error) {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"JSON inválido\"}");
            return;
        }
        
        String ssid = doc["ssid"].as<String>();
        String password = doc["password"].as<String>();
        bool makeDefault = doc["make_default"].as<bool>();
        
        if (ssid.length() == 0 || ssid.length() > 32) {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"SSID inválido\"}");
            return;
        }
        
        NetworkConfig& network_config = config_manager.getNetworkConfig();
        
        Serial.printf("📡 /api/wifi/add: SSID='%s' password_len=%d default=%d\n", ssid.c_str(), password.length(), makeDefault);
        Serial.printf("📡 Redes WiFi ANTES de agregar: %d\n", network_config.getWiFiNetworks().size());
        
        bool success = network_config.addWiFiNetwork(ssid, password, makeDefault);
        
        Serial.printf("📡 addWiFiNetwork resultado: %s\n", success ? "OK" : "FALLO");
        Serial.printf("📡 Redes WiFi DESPUÉS de agregar: %d\n", network_config.getWiFiNetworks().size());
        
        if (success) {
            bool saved = config_manager.save();
            Serial.printf("📡 config_manager.save() resultado: %s\n", saved ? "OK" : "FALLO");
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Red agregada exitosamente\"}");
        } else {
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Error al agregar la red\"}");
        }
    });
    
    // Endpoint para obtener redes guardadas
    server.on("/api/wifi/saved", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!portal_active) {
            request->send(404, "application/json", "{\"success\":false,\"message\":\"Solo disponible en modo portal\"}");
            return;
        }
        
        const NetworkConfig& network_config = config_manager.getNetworkConfig();
        const auto& networks = network_config.getWiFiNetworks();
        
        String json = "{\"success\":true,\"networks\":[";
        bool first = true;
        for (const auto& network : networks) {
            if (!first) json += ",";
            json += "{\"ssid\":\"" + network.ssid + "\",\"is_default\":" + (network.is_default ? "true" : "false") + "}";
            first = false;
        }
        json += "]}";
        
        request->send(200, "application/json", json);
    });
    
    // Endpoint para establecer red predeterminada
    server.on("/api/wifi/default", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL,
    [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (!portal_active) {
            request->send(404, "application/json", "{\"success\":false,\"message\":\"Solo disponible en modo portal\"}");
            return;
        }
        
        String body = "";
        for (size_t i = 0; i < len; i++) {
            body += (char)data[i];
        }
        
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, body);
        if (error) {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"JSON inválido\"}");
            return;
        }
        
        String ssid = doc["ssid"].as<String>();
        NetworkConfig& network_config = config_manager.getNetworkConfig();
        bool success = network_config.setDefaultWiFiNetwork(ssid);
        
        if (success) {
            config_manager.save();
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Red predeterminada establecida\"}");
        } else {
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Error al establecer red predeterminada\"}");
        }
    });
    
    // Endpoint para eliminar red WiFi
    server.on("/api/wifi/remove", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL,
    [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (!portal_active) {
            request->send(404, "application/json", "{\"success\":false,\"message\":\"Solo disponible en modo portal\"}");
            return;
        }
        
        String body = "";
        for (size_t i = 0; i < len; i++) {
            body += (char)data[i];
        }
        
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, body);
        if (error) {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"JSON inválido\"}");
            return;
        }
        
        String ssid = doc["ssid"].as<String>();
        NetworkConfig& network_config = config_manager.getNetworkConfig();
        bool success = network_config.removeWiFiNetwork(ssid);
        
        if (success) {
            config_manager.save();
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Red eliminada exitosamente\"}");
        } else {
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Error al eliminar la red\"}");
        }
    });
    
    // Endpoint para configuración del sensor
    server.on("/api/sensor/config", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL,
    [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        String body = "";
        for (size_t i = 0; i < len; i++) {
            body += (char)data[i];
        }
        
        StaticJsonDocument<768> doc; // Tamaño para todos los campos incluyendo filtro sigma
        DeserializationError error = deserializeJson(doc, body);
        if (error) {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"JSON inválido\"}");
            return;
        }
        
        // 📏 Configuración básica del sensor
        if (doc.containsKey("tank_height")) {
            config_manager.setMaxHeight(doc["tank_height"].as<float>());
        }
        if (doc.containsKey("tank_capacity")) {
            config_manager.setCapacity(doc["tank_capacity"].as<float>());
        }
        if (doc.containsKey("min_distance")) {
            config_manager.setMinDistance(doc["min_distance"].as<float>());
        }
        if (doc.containsKey("container_type")) {
            config_manager.getSensorConfig().setContainerType(doc["container_type"].as<uint8_t>());
        }
        
        // 🔧 Modelo de sensor ultrasónico
        if (doc.containsKey("sensor_model")) {
            uint8_t model = doc["sensor_model"].as<uint8_t>();
            config_manager.getSensorConfig().setSensorModel(model);
            // Si no es personalizado, aplicar distancias del modelo
            if (model != SENSOR_CUSTOM) {
                config_manager.getSensorConfig().applySensorModelDefaults();
                Serial.printf("📝 Modelo sensor: %s (min=%.0f, max=%.0f)\n",
                    config_manager.getSensorConfig().getSensorModelName(),
                    config_manager.getSensorConfig().getMinDistance(),
                    config_manager.getSensorConfig().getMaxDistance());
            }
        }
        if (doc.containsKey("max_distance")) {
            config_manager.getSensorConfig().setMaxDistance(doc["max_distance"].as<float>());
        }
        
        // ⏱️ Configuración de intervalos inteligentes
        if (doc.containsKey("normal_interval")) {
            uint16_t interval = doc["normal_interval"].as<uint16_t>();
            if (interval >= 15 && interval <= 300) {
                config_manager.getSensorConfig().setNormalInterval(interval);
                Serial.printf("📝 Configurado intervalo normal: %ds\n", interval);
            }
        }
        if (doc.containsKey("filling_interval")) {
            uint16_t interval = doc["filling_interval"].as<uint16_t>();
            if (interval >= 3 && interval <= 10) {
                config_manager.getSensorConfig().setFillingInterval(interval);
                Serial.printf("📝 Configurado intervalo llenado: %ds\n", interval);
            }
        }
        if (doc.containsKey("draining_interval")) {
            uint16_t interval = doc["draining_interval"].as<uint16_t>();
            if (interval >= 3 && interval <= 60) {
                config_manager.getSensorConfig().setDrainingInterval(interval);
                Serial.printf("📝 Configurado intervalo vaciado: %ds\n", interval);
            }
        }
        if (doc.containsKey("filling_threshold")) {
            uint8_t threshold = doc["filling_threshold"].as<uint8_t>();
            if (threshold >= 2 && threshold <= 5) {
                config_manager.getSensorConfig().setFillingThreshold(threshold);
                Serial.printf("📝 Configurado umbral llenado: %d lecturas\n", threshold);
            }
        }
        if (doc.containsKey("adaptive_interval")) {
            bool adaptive = doc["adaptive_interval"].as<bool>();
            config_manager.getSensorConfig().setAdaptiveInterval(adaptive);
            // Solo aplicar defaults automáticos si el usuario activa adaptive
            // Y no envió intervalos manuales explícitos en este mismo request
            if (adaptive && !doc.containsKey("normal_interval")) {
                config_manager.getSensorConfig().applyAdaptiveDefaults();
                Serial.printf("📝 Intervalo adaptativo: ON (auto: %ds/%ds/%ds)\n",
                    config_manager.getSensorConfig().getNormalInterval(),
                    config_manager.getSensorConfig().getFillingInterval(),
                    config_manager.getSensorConfig().getDrainingInterval());
            } else {
                Serial.printf("📝 Intervalo adaptativo: %s\n", adaptive ? "ON" : "OFF");
            }
        }
        
        // 🌍 Configuración geográfica NTP
        if (doc.containsKey("auto_geo_location")) {
            config_manager.getSensorConfig().setAutoGeoLocation(doc["auto_geo_location"].as<bool>());
        }
        if (doc.containsKey("timezone")) {
            String timezone = doc["timezone"].as<String>();
            if (timezone.length() > 0) {
                config_manager.getSensorConfig().setTimezone(timezone);
                if (ntpSync) ntpSync->setTimezone(timezone);
                Serial.printf("📝 Configurada zona horaria: %s\n", timezone.c_str());
            }
        }
        if (doc.containsKey("show_datetime")) {
            config_manager.getSensorConfig().setShowDateTime(doc["show_datetime"].as<bool>());
        }
        
        // 📊 Configuración del filtro sigma
        if (doc.containsKey("sigma_samples")) {
            config_manager.getSensorConfig().setSigmaSamples(doc["sigma_samples"].as<uint8_t>());
            Serial.printf("📝 Sigma muestras: %d\n", doc["sigma_samples"].as<uint8_t>());
        }
        if (doc.containsKey("sigma_delay_ms")) {
            config_manager.getSensorConfig().setSigmaDelayMs(doc["sigma_delay_ms"].as<uint16_t>());
            Serial.printf("📝 Sigma delay: %dms\n", doc["sigma_delay_ms"].as<uint16_t>());
        }
        if (doc.containsKey("sigma_threshold")) {
            config_manager.getSensorConfig().setSigmaThreshold(doc["sigma_threshold"].as<float>());
            Serial.printf("📝 Sigma umbral: %.1f\n", doc["sigma_threshold"].as<float>());
        }
        if (doc.containsKey("sigma_min_valid")) {
            config_manager.getSensorConfig().setSigmaMinValid(doc["sigma_min_valid"].as<uint8_t>());
            Serial.printf("📝 Sigma min válidas: %d\n", doc["sigma_min_valid"].as<uint8_t>());
        }
        
        bool success = config_manager.save();
        if (success) {
            Serial.println("✅ Configuración del sensor guardada exitosamente");
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Configuración del sensor guardada exitosamente\"}");
        } else {
            Serial.println("❌ Error al guardar configuración del sensor");
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Error al guardar configuración del sensor\"}");
        }
    });
    
    // 📊 Endpoint GET para obtener configuración actual del sensor
    server.on("/api/sensor/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(2048);
        JsonObject root = doc.to<JsonObject>();
        
        const SensorConfig& sensorConfig = config_manager.getSensorConfig();
        
        // 📏 Configuración básica
        root["tank_height"] = config_manager.getMaxHeight();
        root["tank_capacity"] = config_manager.getCapacity();
        root["min_distance"] = config_manager.getMinDistance();
        root["max_distance"] = sensorConfig.getMaxDistance();
        root["container_type"] = sensorConfig.getContainerTypeAsInt();
        root["sensor_model"] = sensorConfig.getSensorModelAsInt();
        root["sensor_model_name"] = sensorConfig.getSensorModelName();
        
        // ⏱️ Intervalos inteligentes
        root["normal_interval"] = sensorConfig.getNormalInterval();
        root["adaptive_interval"] = sensorConfig.getAdaptiveInterval();
        root["filling_interval"] = sensorConfig.getFillingInterval();
        root["filling_threshold"] = sensorConfig.getFillingThreshold();
        
        // 🌍 Configuración geográfica NTP
        root["auto_geo_location"] = sensorConfig.getAutoGeoLocation();
        root["timezone"] = sensorConfig.getTimezone();
        root["show_datetime"] = sensorConfig.getShowDateTime();
        
        // 📊 Filtro sigma
        root["sigma_samples"] = sensorConfig.getSigmaSamples();
        root["sigma_delay_ms"] = sensorConfig.getSigmaDelayMs();
        root["sigma_threshold"] = sensorConfig.getSigmaThreshold();
        root["sigma_min_valid"] = sensorConfig.getSigmaMinValid();
        
        // 📊 Estado actual
        root["success"] = true;
        
        // 📋 Incluir tabla de modelos de sensores disponibles
        JsonArray models = root.createNestedArray("sensor_models");
        for (uint8_t i = 0; i < SENSOR_SPECS_COUNT; i++) {
            JsonObject m = models.createNestedObject();
            m["id"] = static_cast<uint8_t>(SENSOR_SPECS[i].model);
            m["name"] = SENSOR_SPECS[i].name;
            m["min"] = SENSOR_SPECS[i].min_distance;
            m["max"] = SENSOR_SPECS[i].max_distance;
            m["waterproof"] = SENSOR_SPECS[i].waterproof;
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Endpoint GET para obtener configuración actual del AP
    server.on("/api/ap/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
        Serial.println("📡 GET /api/ap/config");
        
        StaticJsonDocument<256> doc;
        doc["success"] = true;
        doc["ap_ssid"] = config_manager.getAPSSID();
        doc["ap_password"] = config_manager.getAPPassword();
        doc["hostname"] = config_manager.getHostname();
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Endpoint POST para configuración del AP
    server.on("/api/ap/config", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL,
    [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (!portal_active) {
            request->send(404, "application/json", "{\"success\":false,\"message\":\"Solo disponible en modo portal\"}");
            return;
        }
        
        String body = "";
        for (size_t i = 0; i < len; i++) {
            body += (char)data[i];
        }
        
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, body);
        if (error) {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"JSON inválido\"}");
            return;
        }
        
        // Configurar AP
        if (doc.containsKey("ap_ssid")) {
            config_manager.setAPSSID(doc["ap_ssid"].as<String>());
        }
        if (doc.containsKey("ap_password")) {
            config_manager.setAPPassword(doc["ap_password"].as<String>());
        }
        
        bool success = config_manager.save();
        if (success) {
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Configuración del AP guardada\"}");
        } else {
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Error al guardar configuración del AP\"}");
        }
    });
    
    // Endpoint para configuración del sistema
    server.on("/api/system/config", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL,
    [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (!portal_active) {
            request->send(404, "application/json", "{\"success\":false,\"message\":\"Solo disponible en modo portal\"}");
            return;
        }
        
        String body = "";
        for (size_t i = 0; i < len; i++) {
            body += (char)data[i];
        }
        
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, body);
        if (error) {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"JSON inválido\"}");
            return;
        }
        
        // Configurar sistema
        if (doc.containsKey("auto_sleep_time")) {
            config_manager.setAutoSleepTime(doc["auto_sleep_time"].as<uint16_t>());
        }
        
        bool success = config_manager.save();
        if (success) {
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Configuración del sistema guardada\"}");
        } else {
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Error al guardar configuración del sistema\"}");
        }
    });
    
    // ===== HOME ASSISTANT MQTT CONFIGURATION ENDPOINTS =====
    
    // GET: Obtener configuración MQTT actual
    server.on("/api/mqtt/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
        Serial.println("🏠 GET /api/mqtt/config");
        MQTTConfig& mqtt = config_manager.getMQTTConfig();
        
        StaticJsonDocument<512> doc;
        doc["success"] = true;
        doc["brokerHost"] = mqtt.getBrokerHost();
        doc["brokerPort"] = mqtt.getBrokerPort();
        doc["username"] = mqtt.getUsername();
        // Nunca devolver la contraseña: cualquiera en la LAN podía leerla.
        // El POST la conserva si llega vacía (los formularios la reenvían)
        doc["password"] = "";
        doc["hasPassword"] = mqtt.getPassword().length() > 0;
        doc["deviceName"] = mqtt.getDeviceName();
        doc["enabled"] = mqtt.isEnabled();
        doc["configured"] = mqtt.isConfigured();
        
        // Estado de conexión actual
        if (haMqtt) {
            doc["connected"] = haMqtt->isConnected();
            doc["status"] = haMqtt->getStatusString();
        } else {
            doc["connected"] = false;
            doc["status"] = "No inicializado";
        }
        
        String response;
        serializeJson(doc, response);
        AsyncWebServerResponse *resp = request->beginResponse(200, "application/json", response);
        resp->addHeader("Access-Control-Allow-Origin", "*");
        request->send(resp);
    });
    
    // POST: Guardar configuración MQTT
    server.on("/api/mqtt/config", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL,
    [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        Serial.println("🏠 POST /api/mqtt/config");
        
        String body = "";
        for (size_t i = 0; i < len; i++) {
            body += (char)data[i];
        }
        
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, body);
        if (error) {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"JSON inválido\"}");
            return;
        }
        
        MQTTConfig& mqtt = config_manager.getMQTTConfig();
        
        if (doc.containsKey("brokerHost"))  mqtt.setBrokerHost(doc["brokerHost"].as<String>());
        if (doc.containsKey("brokerPort"))  mqtt.setBrokerPort(doc["brokerPort"] | 1883);
        if (doc.containsKey("username"))    mqtt.setUsername(doc["username"].as<String>());
        if (doc.containsKey("password") && doc["password"].as<String>().length() > 0)
            mqtt.setPassword(doc["password"].as<String>());  // Vacía = conservar la actual
        if (doc.containsKey("deviceName"))  mqtt.setDeviceName(doc["deviceName"].as<String>());
        if (doc.containsKey("enabled"))     mqtt.setEnabled(doc["enabled"] | false);
        
        bool saved = config_manager.save();
        
        // Aplicar cambios en caliente al cliente MQTT (vía FreeRTOS task, no-bloqueante)
        if (saved && haMqtt) {
            HomeAssistantMQTT::Config haConfig;
            haConfig.brokerHost    = mqtt.getBrokerHost();
            haConfig.brokerPort    = mqtt.getBrokerPort();
            haConfig.username      = mqtt.getUsername();
            haConfig.password      = mqtt.getPassword();
            haConfig.deviceName    = mqtt.getDeviceName();
            haConfig.reportInterval = 30000;
            
            // Señalizar al FreeRTOS task para que reconfigure
            _mqttPendingConfig = haConfig;
            _mqttPendingEnabled = mqtt.isEnabled();
            _mqttReconfigPending = true;
            Serial.println("🏠 HA MQTT: Reconfiguración señalizada al task");
            
            // Si el task no existe aún, crearlo
            if (!_mqttTaskRunning && mqtt.isEnabled() && mqtt.isConfigured() && wifi_connected) {
                xTaskCreatePinnedToCore(
                    mqttTaskFunc, "HA_MQTT", 8192, this, 0, &_mqttTaskHandle, 0
                );
                Serial.println("🏠 HA MQTT: Task creado desde POST endpoint");
            }
        }
        
        if (saved) {
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Configuración MQTT guardada\"}");
        } else {
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Error al guardar configuración MQTT\"}");
        }
    });
    
    // Endpoint para guardar toda la configuración
    server.on("/api/config/save-all", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (!portal_active) {
            request->send(404, "application/json", "{\"success\":false,\"message\":\"Solo disponible en modo portal\"}");
            return;
        }
        
        bool success = config_manager.save();
        if (success) {
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Toda la configuración guardada exitosamente\"}");
        } else {
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Error al guardar la configuración\"}");
        }
    });
    
    // ===== RUTAS EXCLUSIVAS DEL MODO SENSOR =====
    server.on("/api/sensor-data", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (portal_active) {
            request->send(404, "text/plain", "❌ Sensor data solo disponible en modo sensor");
            return;
        }
        handleSensorData(request);
    });
    
    // Historial de lecturas del sensor (circular buffer)
    server.on("/api/sensor-history", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (portal_active) {
            request->send(404, "text/plain", "Solo disponible en modo sensor");
            return;
        }
        
        // Construir JSON array ordenado cronológicamente (más antiguo primero)
        String json = "{\"count\":" + String(_historyCount) + ",\"entries\":[";
        
        if (_historyCount > 0) {
            // Encontrar el inicio del buffer circular
            uint8_t start = (_historyCount < HISTORY_SIZE) ? 0 : _historyIndex;
            for (uint8_t i = 0; i < _historyCount; i++) {
                uint8_t idx = (start + i) % HISTORY_SIZE;
                const SensorHistoryEntry& e = _sensorHistory[idx];
                if (i > 0) json += ",";
                json += "{\"t\":" + String(e.timestamp);
                json += ",\"l\":" + String(e.litros, 1);
                json += ",\"d\":" + String(e.distancia, 1);
                json += ",\"p\":" + String(e.porcentaje, 1);
                json += ",\"s\":\"" + String(e.estado) + "\"}";
            }
        }
        json += "]}";
        
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
        response->addHeader("Cache-Control", "no-cache");
        request->send(response);
    });
    
    // Detalle de lecturas RAW de la última medición sigma (para dashboard log)
    server.on("/api/last-readings", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (portal_active || !_liquidSensor) {
            request->send(200, "application/json", "{\"error\":\"no disponible\"}");
            return;
        }
        
        uint8_t total = _liquidSensor->getLastTotalSamples();
        const float* raw = _liquidSensor->getLastRawSamples();
        const bool* spike = _liquidSensor->getLastSpikeFlags();
        const bool* sigma = _liquidSensor->getLastSigmaFlags();
        
        // Buffer fijo: 20 readings * ~8 chars + overhead = ~300 bytes
        char buf[512];
        int len = snprintf(buf, sizeof(buf), "{\"id\":%lu,\"n\":%u,\"raw\":[",
            (unsigned long)_sensorReadingId, total);
        
        for (uint8_t i = 0; i < total && len < 380; i++) {
            if (i > 0) buf[len++] = ',';
            len += snprintf(buf + len, sizeof(buf) - len, "%.1f", raw[i]);
        }
        
        len += snprintf(buf + len, sizeof(buf) - len, "],\"f\":\"");
        for (uint8_t i = 0; i < total && len < 440; i++) {
            buf[len++] = raw[i] < 0 ? 'E' : (spike[i] ? 'S' : (sigma[i] ? 'X' : 'O'));
        }
        
        len += snprintf(buf + len, sizeof(buf) - len,
            "\",\"mean\":%.2f,\"std\":%.3f,\"surv\":%u,\"avg\":%.2f,\"rt\":%u,\"rr\":%u}",
            _liquidSensor->getLastMean(), _liquidSensor->getLastStdDev(),
            _liquidSensor->getLastSurvivorCount(), _liquidSensor->getLastFinalAvg(),
            _liquidSensor->getLastRetryCount(), _liquidSensor->getLastRetryRecovered());
        
        request->send(200, "application/json", buf);
    });
    
    server.on("/api/toggle-display", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (portal_active) {
            request->send(404, "text/plain", "❌ Display control solo disponible en modo sensor");
            return;
        }
        handleToggleDisplay(request);
    });
    
    // Auto-sleep: consultar estado
    server.on("/api/auto-sleep", HTTP_GET, [this](AsyncWebServerRequest *request) {
        String json = "{\"autoSleepEnabled\":" + String(display_manager.isAutoSleepEnabled() ? "true" : "false") + "}";
        request->send(200, "application/json", json);
    });
    
    // Auto-sleep: toggle desde dashboard
    server.on("/api/auto-sleep", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (portal_active) {
            request->send(404, "text/plain", "Solo disponible en modo sensor");
            return;
        }
        bool newState = !display_manager.isAutoSleepEnabled();
        display_manager.enableAutoSleep(newState);
        if (!newState && display_manager.isDisplaySleeping()) {
            display_manager.wakeUpDisplay();
            updateSensorDisplay(true); // Usar última lectura en RAM, no hacer nueva lectura
        }
        String json = "{\"autoSleepEnabled\":" + String(newState ? "true" : "false");
        json += ",\"message\":\"Apagado automatico " + String(newState ? "habilitado" : "deshabilitado") + "\"}";
        request->send(200, "application/json", json);
    });
    
    // Modo instalación: toggle desde dashboard
    server.on("/api/installation-mode", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (portal_active) {
            request->send(404, "text/plain", "Solo disponible en modo sensor");
            return;
        }
        setInstallationMode(!_installationMode);
String json = "{\"installationMode\":" + String(_installationMode ? "true" : "false");
        json += ",\"message\":\"Modo instalacion " + String(_installationMode ? "activado" : "desactivado") + "\"}";
        request->send(200, "application/json", json);
        Serial.println(_installationMode ? "🔧 Modo instalacion ACTIVADO (dashboard)" : "✅ Modo instalacion DESACTIVADO (dashboard)");
    });
    
    // Calibrar fondo del contenedor desde dashboard
    server.on("/api/calibrate-bottom", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (portal_active) {
            request->send(404, "text/plain", "Solo disponible en modo sensor");
            return;
        }
        Serial.println("Calibrando fondo del contenedor...");
        
        // Usar la última lectura válida en RAM (no bloquear con sensor_manager.update)
        if (calibrationDistance() >= 0) {
            float d = calibrationDistance();
            if (d > 0) {
                float oldMaxH = config_manager.getMaxHeight();
                config_manager.setMaxHeight(d);
                bool saved = config_manager.save();
                char json[256];
                snprintf(json, sizeof(json),
                    "{\"success\":%s,\"distance\":%.1f,\"previousMaxHeight\":%.1f,"
                    "\"readings\":[%.1f],\"message\":\"Fondo calibrado: %.1f cm\"}",
                    saved ? "true" : "false", d, oldMaxH, d, d);
                request->send(200, "application/json", json);
            } else {
                request->send(500, "application/json", "{\"success\":false,\"message\":\"Lectura invalida\"}");
            }
        } else {
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Sensor sin lectura valida\"}");
        }
    });
    
    // ===== ENDPOINTS DE ACCIONES =====
    server.on("/api/save-and-restart", HTTP_POST, [this](AsyncWebServerRequest *request) {
        Serial.println("🔄 POST /api/save-and-restart");
        
        // Guardar toda la configuración antes de reiniciar
        bool saved = config_manager.save();
        
        if (saved) {
            // Verificar que la config se guardó correctamente leyendo el archivo
            Serial.println("🔍 Verificando config guardada...");
            NetworkConfig verify_nc = config_manager.getNetworkConfig();
            Serial.printf("📋 Redes WiFi en memoria: %d\n", verify_nc.getWiFiNetworks().size());
            for (const auto& net : verify_nc.getWiFiNetworks()) {
                Serial.printf("   📡 '%s' default=%s\n", net.ssid.c_str(), net.is_default ? "SÍ" : "no");
            }
            
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Configuración guardada. Reiniciando dispositivo...\"}");
            
            // Escribir flag para forzar modo normal en el próximo arranque
            // Esto garantiza que el ESP32 intente WiFi sin importar DRD
            setForceNormalBoot();
            
            // Limpiar flag de DRD en LittleFS para que el próximo arranque sea normal
            Serial.println("🔄 Limpiando DRD flag antes de reiniciar...");
            drd.stop();
            Serial.println("✅ DRD flag limpiado. Reiniciando en 3 segundos...");
            delay(3000);  // 3 segundos para asegurar que LittleFS flush completa
            ESP.restart();
        } else {
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Error al guardar la configuración\"}");
        }
    });
    
    server.on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest *request) {
        Serial.println("🔄 POST /api/restart");
        
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Reiniciando dispositivo...\"}");
        
        // Si hay redes WiFi configuradas, forzar modo normal al reiniciar
        NetworkConfig nc = config_manager.getNetworkConfig();
        if (!nc.getWiFiNetworks().empty()) {
            setForceNormalBoot();
        }
        
        // Limpiar flag de DRD en LittleFS
        Serial.println("🔄 Limpiando DRD flag antes de reiniciar...");
        drd.stop();
        
        // Reiniciar después de un delay para enviar la respuesta
        Serial.println("🔄 Reiniciando dispositivo en 3 segundos...");
        delay(3000);
        ESP.restart();
    });
    
    server.on("/api/factory-reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
        Serial.println("🗑️ POST /api/factory-reset");
        
        // Borrar toda la configuración
        config_manager.reset();
        bool reset = config_manager.save();
        
        if (reset) {
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Reset de fábrica completado. Reiniciando...\"}");
            
            // Forzar portal cautivo tras reinicio (ya no hay redes guardadas)
            Serial.println("📡 Forzando portal cautivo tras factory-reset...");
            setForcePortalBoot();
            drd.stop();
            
            // Reiniciar después de un delay para enviar la respuesta
            Serial.println("✅ Reset de fábrica completado. Reiniciando en 3 segundos...");
            delay(3000);
            ESP.restart();
        } else {
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Error al realizar reset de fábrica\"}");
        }
    });
    
    // Reset solo WiFi (desde botón "Reset WiFi" del dashboard)
    server.on("/api/reset-wifi", HTTP_POST, [this](AsyncWebServerRequest *request) {
        Serial.println("📶 POST /api/reset-wifi");
        
        // Borrar solo las redes WiFi (no el resto de config)
        config_manager.getNetworkConfig().clearWiFiNetworks();
        config_manager.save();
        
        // Borrar credenciales NVS de ESP32
        WiFi.disconnect(true, true);
        
        Serial.println("📶 Redes WiFi borradas. Forzando portal cautivo...");
        request->send(200, "application/json", "{\"success\":true,\"message\":\"WiFi reiniciado. Entrando en modo configuración...\"}");
        
        // Forzar portal cautivo en el próximo arranque
        setForcePortalBoot();
        drd.stop();
        
        delay(2000);
        ESP.restart();
    });
    
    // ===== API DASHBOARD UNIFICADO (tinacos remotos por ESP-NOW) =====
    server.on("/multi-sensor-data", HTTP_GET, [this](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(2048);
        doc["timestamp"] = millis();
        JsonObject net = doc.createNestedObject("network");
        net["status"] = (espNowManager && espNowManager->isInit()) ? "active" : "inactive";
        net["master_device"] = WiFi.macAddress();
        net["connected_count"] = espNowManager ? espNowManager->getConnectedSensorCount() : 0;
        JsonArray slaves = doc.createNestedArray("slaves");
        if (espNowManager) {
            for (const auto& s : espNowManager->getConnectedSensors()) {
                uint32_t age = millis() - s.timestamp;
                JsonObject o = slaves.createNestedObject();
                o["id"] = "SLAVE_" + String(s.sensorId);
                o["name"] = s.sensorName;
                o["percentage"] = serialized(String(s.liquidLevel, 1));
                o["liquidLevel"] = String(s.liters, 1);
                o["distance"] = String(s.distance, 1);
                o["rssi"] = s.rssi;
                o["lastSeen"] = String(s.timestamp);
                o["mac"] = espNowManager->macToString(s.mac);
                o["firmware"] = "ESP-NOW";
                o["status"] = age < 300000 ? "online" : (age < ESPNOW_REMOTE_FRESH_MS ? "warning" : "offline");
            }
        }
        String json;
        serializeJson(doc, json);
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });

    // Estado completo: modo, rol, canal, unidades, tinacos remotos y contadores de filtrado.
    // Nunca incluye la clave (solo key_set)
    server.on("/api/esp-now/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", espNowStatusJson());
        response->addHeader("Cache-Control", "no-cache");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });

    // ===== ESP-NOW MANAGEMENT ENDPOINTS =====
    server.on("/api/esp-now/info", HTTP_GET, [this](AsyncWebServerRequest *request) {
        Serial.println("🔍 Procesando solicitud de información ESP-NOW");

        String jsonResponse = "{";
        jsonResponse += "\"status\":\"active\",";
        jsonResponse += "\"version\":\"" + String(FIRMWARE_VERSION) + "\",";
        jsonResponse += "\"device_name\":\"" + config_manager.getHostname() + "\",";
        jsonResponse += "\"firmware\":\"ESP32_LiquidLevel_v" + String(FIRMWARE_VERSION) + "\",";
        jsonResponse += "\"uptime\":" + String(millis()) + ",";
        jsonResponse += "\"free_heap\":" + String(ESP.getFreeHeap());
        jsonResponse += "}";

        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonResponse);
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });

    // Modo, rol, clave y nombre. Clave vacía = conservar la actual (como la contraseña MQTT).
    // Los cambios se aplican en loop() en el siguiente segundo
    server.on("/api/esp-now/configure", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            int body = collectBody(request, data, len, index, total);
            if (body == 0) return;
            StaticJsonDocument<384> doc;
            if (body < 0 || deserializeJson(doc, (const char*)request->_tempObject)) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"JSON invalido\"}");
                return;
            }
            EspNowMode mode = ESPNOW_MODE_AUTO;
            EspNowRole role = ESPNOW_ROLE_EMISOR;
            bool hasMode = doc.containsKey("mode");
            bool hasRole = doc.containsKey("role");
            if (hasMode && !ESPNowConfig::modeFromString(doc["mode"].as<String>(), mode)) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Modo invalido (apagado, manual o automatico)\"}");
                return;
            }
            if (hasRole && !ESPNowConfig::roleFromString(doc["role"].as<String>(), role)) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Rol invalido (emisor o receptor)\"}");
                return;
            }
            String key = doc["key"] | "";
            if (key.length() > 0 && (key.length() < ESPNowConfig::KEY_MIN_LEN || key.length() > ESPNowConfig::KEY_MAX_LEN)) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"La clave debe tener entre 8 y 64 caracteres\"}");
                return;
            }
            String name = doc["sensorName"] | (doc["name"] | "");
            if (name.length() > 31) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Nombre de hasta 31 caracteres\"}");
                return;
            }
            queueEspNowRequest([&](EspNowRequest& r) {
                if (hasMode) { r.setMode = true; r.mode = mode; }
                if (hasRole) { r.setRole = true; r.role = role; }
                if (key.length() > 0) { r.setKey = true; r.key = key; }
                if (name.length() > 0) { r.setName = true; r.name = name; }
            });
            request->send(200, "application/json", "{\"status\":\"configured\",\"message\":\"Configuracion ESP-NOW aplicada\"}");
        });

    // Ventana de emparejamiento de 3 min (radio despierta). Hay que abrirla en las dos
    // unidades, con la misma clave: solo entonces se aceptan unidades nuevas
    auto pairHandler = [this](AsyncWebServerRequest *request) {
        const ESPNowConfig& cfg = config_manager.getESPNowConfig();
        if (cfg.getMode() == ESPNOW_MODE_OFF) {
            request->send(409, "application/json", "{\"status\":\"error\",\"message\":\"ESP-NOW esta Apagado: elige Manual o Automatico\"}");
            return;
        }
        if (!cfg.hasPairingKey()) {
            request->send(409, "application/json", "{\"status\":\"error\",\"message\":\"Primero guarda la clave de emparejamiento (la misma en ambas unidades)\"}");
            return;
        }
        _espNowPairingRequested = true;
        request->send(200, "application/json", "{\"status\":\"pairing\",\"message\":\"Emparejamiento abierto 3 min\",\"duration\":" +
                      String(ESPNOW_PAIRING_MS / 1000) + "}");
    };
    server.on("/api/esp-now/pair", HTTP_POST, pairHandler);
    server.on("/api/esp-now/scan", HTTP_POST, pairHandler);  // Nombre anterior

    // Agregar / quitar una unidad por MAC (sin la clave correcta sus tramas se siguen descartando)
    // -1 = MAC inválida, 0 = faltan trozos del cuerpo, 1 = MAC leída
    auto macFromBody = [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total, uint8_t* mac) {
        int body = collectBody(request, data, len, index, total);
        if (body <= 0) return body;
        StaticJsonDocument<128> doc;
        if (deserializeJson(doc, (const char*)request->_tempObject)) return -1;
        return ESPNowManager::getInstance()->stringToMac(doc["mac"] | "", mac) ? 1 : -1;
    };
    server.on("/api/esp-now/connect", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL,
        [this, macFromBody](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            uint8_t mac[6];
            int r = macFromBody(request, data, len, index, total, mac);
            if (r == 0) return;
            if (r < 0) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"MAC invalida\"}");
                return;
            }
            queueEspNowRequest([&](EspNowRequest& r) { r.addPeer = true; memcpy(r.addMac, mac, 6); });
            request->send(200, "application/json", "{\"status\":\"connected\",\"message\":\"Unidad agregada\"}");
        });
    server.on("/api/esp-now/disconnect", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL,
        [this, macFromBody](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            uint8_t mac[6];
            int r = macFromBody(request, data, len, index, total, mac);
            if (r == 0) return;
            if (r < 0) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"MAC invalida\"}");
                return;
            }
            queueEspNowRequest([&](EspNowRequest& r) { r.removePeer = true; memcpy(r.removeMac, mac, 6); });
            request->send(200, "application/json", "{\"status\":\"disconnected\",\"message\":\"Unidad eliminada\"}");
        });

    server.on("/api/esp-now/ping", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL,
        [this, macFromBody](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            uint8_t mac[6];
            int r = macFromBody(request, data, len, index, total, mac);
            if (r == 0) return;
            if (!espNowManager || !espNowManager->isInit()) {
                request->send(503, "application/json", "{\"status\":\"error\",\"message\":\"ESP-NOW no inicializado\"}");
                return;
            }
            if (r < 0) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"MAC invalida\"}");
                return;
            }
            bool sent = espNowManager->sendPing(mac);
            request->send(200, "application/json", "{\"status\":\"" + String(sent ? "success" : "failed") +
                          "\",\"ping_sent\":" + String(sent ? "true" : "false") + "}");
        });

    // Pruebas: ?kind=ping (defecto) | replay (reenvía la última trama tal cual) |
    // badkey (trama firmada con otra clave). El otro equipo debe contarlas en
    // stats.rx_replay / stats.rx_bad_auth de /api/esp-now/status
    // kind=wifioff&s=N (60–1800): caída de WiFi simulada para probar el respaldo Automático
    // sin desconectar el router. La unidad no responde por HTTP hasta que vence
    server.on("/api/esp-now/test", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (request->hasParam("kind") && request->getParam("kind")->value() == "wifioff") {
            long s = request->hasParam("s") ? request->getParam("s")->value().toInt() : 420;
            if (s < 60 || s > 1800) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"s entre 60 y 1800\"}");
                return;
            }
            _wifiSimRequestMs = (uint32_t)s * 1000UL;
            request->send(200, "application/json", "{\"status\":\"scheduled\",\"kind\":\"wifioff\",\"seconds\":" + String(s) + "}");
            return;
        }
        if (!espNowManager || !espNowManager->isInit()) {
            request->send(503, "application/json", "{\"status\":\"error\",\"message\":\"ESP-NOW no inicializado\"}");
            return;
        }
        String kind = request->hasParam("kind") ? request->getParam("kind")->value() : String("ping");
        bool ok;
        if (kind == "replay") ok = espNowManager->testReplayLast();
        else if (kind == "badkey") ok = espNowManager->testBadKey();
        else { kind = "ping"; ok = espNowManager->pingAll(); }
        request->send(200, "application/json", "{\"status\":\"" + String(ok ? "sent" : "failed") +
                      "\",\"kind\":\"" + kind + "\",\"packets_sent\":" + String(espNowManager->getPacketsSent()) + "}");
        Serial.printf("🧪 Prueba ESP-NOW '%s': %s\n", kind.c_str(), ok ? "enviada" : "fallo");
    });

    server.on("/api/esp-now/reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
        _espNowResetRequested = true;
        request->send(200, "application/json", "{\"status\":\"reset_ok\",\"message\":\"ESP-NOW se reinicia en el siguiente segundo\"}");
    });
}

void AppManager::setupCaptiveDetectionRoutes() {
    Serial.println("📡 Configurando detección de portal cautivo...");
    
    // ===== DETECCIÓN ANDROID =====
    server.on("/generate_204", HTTP_GET, [this](AsyncWebServerRequest *request) {
        Serial.println("📱 Android detectó portal cautivo: /generate_204");
        if (portal_active) {
            request->send(200, "text/html", "<script>window.location.href='/';</script>");
        } else {
            request->send(204); // No content en modo sensor
        }
    });
    
    server.on("/gen_204", HTTP_GET, [this](AsyncWebServerRequest *request) {
        Serial.println("📱 Android detectó portal cautivo: /gen_204");
        if (portal_active) {
            request->send(200, "text/html", "<script>window.location.href='/';</script>");
        } else {
            request->send(204);
        }
    });
    
    // ===== DETECCIÓN iOS =====
    server.on("/hotspot-detect.html", HTTP_GET, [this](AsyncWebServerRequest *request) {
        Serial.println("🍎 iOS detectó portal cautivo: /hotspot-detect.html");
        if (portal_active) {
            request->send(200, "text/html", "<script>window.location.href='/';</script>");
        } else {
            request->send(200, "text/html", "<html><body>Success</body></html>");
        }
    });
    
    // ===== CATCH-ALL INTELIGENTE =====
    server.onNotFound([this](AsyncWebServerRequest *request) {
        String url = request->url();
        Serial.printf("❓ Ruta no encontrada: %s (Modo: %s)\n", url.c_str(), 
                      portal_active ? "PORTAL" : "SENSOR");
        
        if (portal_active) {
            // En modo portal, redirigir todo al portal
            String redirectHTML = "<!DOCTYPE html><html><head><title>Portal Cautivo</title></head>"
                                 "<body><script>window.location.href='/';</script>"
                                 "<p>Redirigiendo al portal...</p></body></html>";
            request->send(200, "text/html", redirectHTML);
        } else {
            // En modo sensor, devolver 404 normal
            request->send(404, "text/plain", "❌ Página no encontrada");
        }
    });
}

void AppManager::initializeServices() {
    Serial.println("🔧 Inicializando servicios específicos...");
    
    if (portal_active) {
        Serial.println("🌐 Modo Portal - Servicios básicos");
        // En modo portal solo lo esencial
        
    } else {
        Serial.println("🌊 Modo Sensor - Servicios completos");
        // En modo sensor, habilitar WebSerial y OTA
        WebSerial.begin(&server);
        
        // Registrar callback para procesar comandos recibidos por WebSerial
        WebSerial.onMessage([this](uint8_t *data, size_t len) {
            String command = "";
            for (size_t i = 0; i < len; i++) {
                command += (char)data[i];
            }
            command.trim();
            command.toLowerCase();
            Serial.println("📟 WebSerial comando: " + command);
            
            // Consolidar toda la salida en un solo String para evitar desync WebSocket
            String output = "";
            
            if (command == "status") {
                output += "=== Estado del Sistema ===\n";
                output += "WiFi: " + WiFi.SSID() + "\n";
                output += "IP: " + WiFi.localIP().toString() + "\n";
                output += "RSSI: " + String(WiFi.RSSI()) + " dBm\n";
                output += "Heap libre: " + String(ESP.getFreeHeap()) + " bytes\n";
                output += "Temp interna: " + String(temperatureRead(), 1) + " C (aprox)\n";
                output += "CPU: " + String(getCpuFrequencyMhz()) + " MHz | Modem sleep: " + String(_wifiPowerSave ? "ON" : "OFF") + "\n";
                {
                    const ESPNowConfig& ec = config_manager.getESPNowConfig();
                    output += "ESP-NOW: " + String(ESPNowConfig::modeToString(ec.getMode())) + ", " +
                              ESPNowConfig::roleToString(ec.getRole()) + ", " +
                              String((espNowManager && espNowManager->isInit()) ? "encendido" : "apagado") +
                              (ec.hasPairingKey() ? "" : " (sin clave)") + ", " +
                              String(ec.getPeers().size()) + " unidades, canal guardado " + String(ec.getLastChannel()) +
                              " (detalle en /espnow)\n";
                }
                output += "Uptime: " + String(millis() / 1000) + " seg";
            } else if (command == "sensor") {
                String sensorJson = sensor_manager.getSensorJson("LiquidLevel");
                output += "Sensor: " + sensorJson;
            } else if (command == "network" || command == "net") {
                String hn = String(WiFi.getHostname());
                output += "=== Configuracion de Red ===\n";
                output += "SSID: " + WiFi.SSID() + "\n";
                output += "IP: " + WiFi.localIP().toString() + "\n";
                output += "Subnet: " + WiFi.subnetMask().toString() + "\n";
                output += "Gateway: " + WiFi.gatewayIP().toString() + "\n";
                output += "DNS 1: " + WiFi.dnsIP(0).toString() + "\n";
                output += "DNS 2: " + WiFi.dnsIP(1).toString() + "\n";
                output += "MAC: " + WiFi.macAddress() + "\n";
                output += "Canal: " + String(WiFi.channel()) + "\n";
                output += "RSSI: " + String(WiFi.RSSI()) + " dBm\n";
                output += "Hostname: " + hn + "\n";
                output += "mDNS: http://" + hn + ".local\n";
                output += "--- Config AP (portal cautivo) ---\n";
                output += "AP SSID: " + config_manager.getAPSSID() + "\n";
                output += "AP IP: 192.168.1.1";
            } else if (command == "monitor" || command == "mon") {
                _liveMonitorEnabled = !_liveMonitorEnabled;
                if (_liveMonitorEnabled) {
                    output += "📡 Monitor en tiempo real ACTIVADO\n";
                    output += "Escribe 'monitor' de nuevo para detener\n";
                    
                    // Lectura inmediata usando datos en RAM (sin sensor_manager.update)
                    if (_liquidSensor && _liquidSensor->hasValidReading()) {
                        float d = _liquidSensor->getDistance();
                        float p = _liquidSensor->getPercentage();
                        float l = _liquidSensor->getLitros();
                        String ts = "";
                        if (ntpSync && ntpSync->isReady()) {
                            ts = ntpSync->getTimeWithMsString();
                        }
                        output += "📡 ";
                        if (ts.length() > 0) output += "[" + ts + "] ";
                        output += String(l, 1) + "L | " + String(d, 1) + "cm | " + String(p, 1) + "%\n";
                        output += "\n" + _liquidSensor->getLastMeasurementDetails();
                    } else {
                        output += "(sensor sin lectura valida aun)";
                    }
                } else {
                    output += "⏹️ Monitor en tiempo real DESACTIVADO";
                }
            } else if (command == "restart" || command == "reboot") {
                WebSerial.println("Reiniciando...");
                vTaskDelay(pdMS_TO_TICKS(500)); // Dar tiempo al WebSocket de enviar
                ESP.restart();
            } else if (command == "instalar" || command == "install") {
                setInstallationMode(!_installationMode);
                if (_installationMode) {
                    output += "🔧 MODO INSTALACION ACTIVADO\n";
                    output += "Distancia cruda (sin filtro ni cálculos) ~1 lectura/s\n";
                    output += "También en http://" + WiFi.localIP().toString() + "/instalacion\n";
                    output += "Posicione el sensor en la tapa del contenedor\n";
                    output += "Escribe 'instalar' para salir del modo\n";
                    output += "---\n";
                    // Lectura inmediata desde RAM (sin sensor_manager.update)
                    if (_liquidSensor && _liquidSensor->hasValidReading()) {
                        output += "📏 Distancia: " + String(_liquidSensor->getDistance(), 1) + " cm";
                    } else {
                        output += "(esperando primera lectura...)";
                    }
                } else {
                    output += "✅ Modo instalacion DESACTIVADO\n";
                    output += "Filtro reiniciado; lectura normal inmediata";
                }
            } else if (command == "calibrar" || command == "calibrate") {
                // Calibrar fondo usando la última lectura válida en RAM (sin bloquear)
                output += "📐 CALIBRANDO FONDO DEL CONTENEDOR...\n";
                
                if (calibrationDistance() >= 0) {
                    float d = calibrationDistance();
                    if (d > 0) {
                        float oldMaxH = config_manager.getMaxHeight();
                        config_manager.setMaxHeight(d);
                        bool saved = config_manager.save();
                        output += "  Lectura actual: " + String(d, 1) + " cm\n";
                        if (saved) {
                            output += "---\n";
                            output += "✅ Fondo calibrado exitosamente!\n";
                            output += "   Distancia al fondo: " + String(d, 1) + " cm\n";
                            output += "   Valor anterior: " + String(oldMaxH, 1) + " cm\n";
                            output += "   (maxHeight actualizado y guardado)";
                        } else {
                            output += "❌ Error guardando calibracion";
                        }
                    } else {
                        output += "❌ Lectura invalida (0 cm)";
                    }
                } else {
                    output += "❌ Sensor sin lectura valida\n";
                    output += "Verifique que el sensor este conectado";
                }
            } else if (command == "config") {
                SensorConfig sc = config_manager.getSensorConfig();
                output += "=== Configuracion ===\n";
                output += "Hostname: " + config_manager.getHostname() + "\n";
                output += "AP SSID: " + config_manager.getAPSSID() + "\n";
                output += "Modelo sensor: " + String(sc.getSensorModelName()) + "\n";
                output += "Max Height: " + String(config_manager.getMaxHeight()) + "\n";
                output += "Min Distance: " + String(config_manager.getMinDistance()) + "\n";
                output += "Capacity: " + String(config_manager.getCapacity()) + " L\n";
                output += "Intervalo normal: " + String(sc.getNormalInterval()) + "s\n";
                output += "Intervalo llenado: " + String(sc.getFillingInterval()) + "s\n";
                output += "Auto-sleep: " + String(config_manager.getAutoSleepTime()) + "s";
            } else if (command == "llenar" || command == "filling") {
                _forceFillingMode = !_forceFillingMode;
                if (_forceFillingMode) {
                    _forceFillingTimestamp = millis();
                    output += "=== MODO LLENADO FORZADO ACTIVADO ===\n";
                    output += "Intervalo de lectura acelerado\n";
                    output += "Auto-timeout: 2 minutos si no se detecta llenado real\n";
                    output += "Escribe 'llenar' para desactivar manualmente";
                } else {
                    output += "=== Modo llenado forzado DESACTIVADO ===\n";
                    output += "Volviendo a deteccion automatica";
                }
            } else if (command == "help" || command == "?") {
                output += "=== Comandos Disponibles ===\n";
                output += "status   - Estado del sistema\n";
                output += "sensor   - Datos del sensor\n";
                output += "network  - Config de red (IP, MAC, DNS, etc)\n";
                output += "monitor  - Sensado en tiempo real (toggle)\n";
                output += "llenar   - Forzar modo llenado rapido (toggle, 2min auto)\n";
                output += "instalar - Modo instalacion: distancia RAW (toggle)\n";
                output += "calibrar - Calibrar fondo del contenedor vacio\n";
                output += "config   - Configuracion del sensor\n";
                output += "restart  - Reiniciar ESP32\n";
                output += "help     - Esta ayuda";
            } else {
                output += "Comando desconocido: " + command + "\n";
                output += "Escribe 'help' para ver comandos disponibles";
            }
            
            if (output.length() > 0) {
                WebSerial.println(output);
            }
        });
        Serial.println("✅ WebSerial habilitado con procesamiento de comandos");
        
        ElegantOTA.begin(&server);
        Serial.println("✅ WebOTA habilitado");
    }
}

void AppManager::apCallback() {
    Serial.println("=== AP CALLBACK - INFO INMEDIATA ===");
    
    // Obtener configuración actual del AP
    String ap_ssid = config_manager.getAPSSID();
    String ap_password = config_manager.getAPPassword();
    String ap_ip = WiFi.softAPIP().toString();
    String ap_mac = WiFi.softAPmacAddress();
    
    Serial.println("AP SSID: " + ap_ssid);
    Serial.println("AP Password: " + ap_password);
    Serial.println("AP IP: " + ap_ip);
    Serial.println("AP MAC: " + ap_mac);
    
    updatePortalDisplay();
    
    Serial.println("Portal disponible en: http://" + ap_ip);
}

void AppManager::updatePortalDisplay() {
    // Obtener información del AP
    String ap_ssid = config_manager.getAPSSID();
    String ap_password = config_manager.getAPPassword();
    String ap_ip = WiFi.softAPIP().toString();
    String ap_mac = WiFi.softAPmacAddress();
    int numClients = WiFi.softAPgetStationNum();
    
    // Mostrar información en display
    display_manager.clear();
    display_manager.setFont(ArialMT_Plain_10);
    display_manager.setTextAlignment(TEXT_ALIGN_LEFT);
    
    // Espaciado de 11px por línea para que las 6 líneas quepan en 64px de alto
    display_manager.drawString(0, 0, "PORTAL CAUTIVO");
    display_manager.drawString(0, 11, "Red: " + ap_ssid);
    display_manager.drawString(0, 22, "Pass: " + ap_password);
    display_manager.drawString(0, 33, "IP: " + ap_ip);
    display_manager.drawString(0, 44, "MAC: " + ap_mac);
    
    // Línea de clientes conectados (y=54 + 10px font = 64px, cabe exacto)
    String clientsInfo = "Clientes: " + String(numClients);
    display_manager.drawScrollingText(0, 54, clientsInfo, 128);
    
    display_manager.display();
}

void AppManager::loop() {
    // Reset watchdog para evitar reinicios por timeout
    esp_task_wdt_reset();
    
    drd.loop();
    yield(); // Ceder CPU a WiFi stack frecuentemente
    
    // Actualizar sincronización NTP
    if (ntpSync) {
        ntpSync->loop();
    }
    
    // WebSerial: vaciar buffers internos para que las respuestas lleguen al cliente
    // Sin esto, los datos quedan en el doble buffer (print→global→WebSocket) y no se envían
    if (!portal_active) {
        WebSerial.loop();
        ElegantOTA.loop();  // Reinicio automático 2s después de un OTA exitoso
    }
    yield(); // Ceder CPU a WiFi stack
    
    // Procesar DNS requests solo en portal cautivo (evita uso innecesario de CPU en modo sensor)
    if (portal_active) {
        dnsServer.processNextRequest();
    }
    
    // Actualizar scroll de texto en portal cautivo
    if (portal_active) {
        display_manager.updateScrollingText();
        
        // Actualizar clientes conectados cada 3 segundos
        static unsigned long lastPortalUpdate = 0;
        if (millis() - lastPortalUpdate > 3000) {
            updatePortalDisplay();
            lastPortalUpdate = millis();
        }
        
        // Auto-reboot: si el portal lleva AP_IDLE_REBOOT_MS sin clientes
        // y hay redes WiFi guardadas, reiniciar para intentar modo normal.
        // No aplica si el usuario forzó el portal (doble reset).
        if (!forcePortalMode && _portalStartTime > 0 &&
            (millis() - _portalStartTime) >= AP_IDLE_REBOOT_MS) {
            uint8_t clients = WiFi.softAPgetStationNum();
            if (clients == 0) {
                NetworkConfig nc = config_manager.getNetworkConfig();
                if (!nc.getWiFiNetworks().empty()) {
                    Serial.println("[PORTAL] ⏰ 5 min sin clientes y hay WiFi guardada - reiniciando...");
                    delay(100);
                    ESP.restart();
                }
            } else {
                // Hay clientes conectados: renovar el timer
                _portalStartTime = millis();
            }
        }
    }
    
    // ESP-NOW: recepción autenticada y emparejamiento (cada vuelta); modo, respaldo sin
    // WiFi, reconexión espaciada y modem sleep (cada 1 s)
    if (!portal_active && espNowManager) {
        espNowManager->loop();
        if (_wifiRetryInProgress &&
            (_wifiRetryEnded || millis() - _wifiRetryStart >= WIFI_RETRY_TIMEOUT_MS)) {
            finishWifiRetry();
        }
        static unsigned long lastEspNowCheck = 0;
        if (millis() - lastEspNowCheck >= 1000) {
            updateEspNow();
            lastEspNowCheck = millis();
            // Receptor con tinacos remotos: refrescar la línea alterna del OLED
            if (!_installationMode && !display_manager.isDisplaySleeping() &&
                espNowManager->isInit() && config_manager.getESPNowConfig().isReceptor() &&
                espNowManager->getConnectedSensorCount() > 0) {
                updateSensorDisplay();
            }
        }
    }

    // Monitoreo de memoria cada 10 segundos
    static unsigned long lastMemCheck = 0;
    if (millis() - lastMemCheck > 10000) {
        size_t freeHeap = ESP.getFreeHeap();
        if (freeHeap < 25000) {
            Serial.printf("⚠️ ADVERTENCIA: Memoria baja: %d bytes\n", freeHeap);
            // Forzar limpieza de escaneos WiFi si la memoria está baja
            if (WiFi.scanComplete() >= 0) {
                WiFi.scanDelete();
                Serial.println("🧹 Limpieza de escaneo WiFi por memoria baja");
            }
        }
        lastMemCheck = millis();
    }
    
    // Chequeo de conexión WiFi después de guardar configuración
    if (checkingConnection && millis() > connectionCheckTimer) {
        checkingConnection = false;
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("✅ Conectado exitosamente a WiFi!");
            Serial.println("📍 IP asignada: " + WiFi.localIP().toString());
            wifi_connected = true;
            portal_active = false;
            
            // Desactivar portal cautivo: SoftAP y DNS
            dnsServer.stop();
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_STA);
            updateWiFiPowerSave(true);
            Serial.println("📡 Portal cautivo desactivado - Transición a modo sensor");
            
            // Mostrar info de sensor en OLED
            display_manager.clear();
            display_manager.setFont(ArialMT_Plain_10);
            display_manager.setTextAlignment(TEXT_ALIGN_LEFT);
            display_manager.drawString(0, 0, "SENSOR ACTIVO");
            display_manager.drawString(0, 12, "WiFi: " + WiFi.SSID());
            display_manager.drawString(0, 24, "IP: " + WiFi.localIP().toString());
            display_manager.drawString(0, 36, "Leyendo sensor...");
            display_manager.display();
            
            // Habilitar auto-sleep en modo sensor
            display_manager.enableAutoSleep(true);
            Serial.println("🖥️ Modo sensor activado - Auto-sleep HABILITADO");
        } else {
            Serial.println("❌ No se pudo conectar a WiFi. Reintentando...");
            Serial.printf("📊 Estado WiFi: %d\n", WiFi.status());
        }
    }
    
    // Manejar botón PRG para despertar pantalla
    bool wasSleeping = display_manager.isDisplaySleeping();
    display_manager.handlePRGButton();
    
    // Si la pantalla acaba de despertar, forzar redibujado completo del sensor
    if (wasSleeping && !display_manager.isDisplaySleeping() && !portal_active) {
        Serial.println("🔆 Pantalla despertada - forzando redibujado completo");
        sensor_manager.update();
        updateSensorDisplay(true);
    }
    
    // Chequear auto-sleep de pantalla (solo en modo sensor)
    display_manager.checkAutoSleep();
    yield(); // Ceder CPU antes de lectura de sensor
    
    // ===== HA MQTT: Se ejecuta en FreeRTOS task separado (Core 0) =====
    // Ya no bloquea el loop principal — ver mqttTaskFunc()
    
    // ===== LECTURA INTELIGENTE DEL SENSOR (solo en modo normal) =====
    if (!portal_active) {
        static unsigned long lastSensorRead = 0;
        static float lastReadings[10] = {0}; // Historial de últimas lecturas
        static uint8_t readingIndex = 0;
        static bool isFillingDetected = false;
        static bool isDrainingDetected = false;
        
        // Tras salir del modo instalación: el historial de flujo es de antes de mover el sensor
        if (_resetFlowHistory) {
            memset(lastReadings, 0, sizeof(lastReadings));
            readingIndex = 0;
            isFillingDetected = false;
            isDrainingDetected = false;
            _resetFlowHistory = false;
        }

        // ===== MODO INSTALACIÓN: distancia cruda ~1 lectura/s =====
        // Mediana de 5 muestras rápidas, sin spike guard / sigma / estabilidad: el sensor
        // se está moviendo y el filtro rechazaría los cambios o activaría el failsafe
        if (_installationMode) {
            if (millis() - lastSensorRead > INSTALL_READ_INTERVAL_MS) {
                float d = _liquidSensor ? _liquidSensor->measureRawQuick() : -1.0f;
                _installRawCm = d;
                _sensorReadingId++;
                String line = (d < 0) ? String("Dist: SIN ECO {Inst}") : "Dist: " + String(d, 1) + " cm {Inst}";
                WebSerial.println(line);
                Serial.println(line);
                updateInstallationDisplay(d);
                lastSensorRead = millis();
            }
            yield();
            return; // No ejecutar lógica normal del sensor
        }

        // Obtener configuración de intervalos
        SensorConfig sensorConfig = config_manager.getSensorConfig();
        uint32_t currentInterval;
        
        // Si adaptive está activo, recalcular intervalos según contenedor
        if (sensorConfig.getAdaptiveInterval()) {
            // Los intervalos adaptivos ya están aplicados en la config
        }
        
        if (_liveMonitorEnabled) {
            // Con monitor activo: leer cada 5s para actualización rápida en WebSerial
            currentInterval = 5000;
        } else if (isFillingDetected) {
            currentInterval = (uint32_t)sensorConfig.getFillingInterval() * 1000UL;
        } else if (isDrainingDetected) {
            currentInterval = (uint32_t)sensorConfig.getDrainingInterval() * 1000UL;
        } else {
            currentInterval = (uint32_t)sensorConfig.getNormalInterval() * 1000UL;
        }
        
        // Primera lectura inmediata al arrancar: antes había que esperar un intervalo
        // completo (120s) y la pantalla quedaba en "Leyendo sensor..." hasta entonces
        static bool firstReadDone = false;
        if (!firstReadDone || _forceSensorRead || millis() - lastSensorRead > currentInterval) {
            firstReadDone = true;
            _forceSensorRead = false;
// Leer timestamp NTP si disponible
            String timeStr = "";
            if (ntpSync && ntpSync->isReady()) {
                timeStr = ntpSync->getCurrentDateTimeString();
            }
            // Log reducido (sin emoji para menor latencia Serial)
            Serial.printf("[%s] Sensor (%s %lus)\n",
                         timeStr.length() > 0 ? timeStr.c_str() : String(millis()/1000).c_str(),
                         _liveMonitorEnabled ? "MON" : (isFillingDetected ? "FILL" : (isDrainingDetected ? "DRAIN" : "NRM")),
                         (unsigned long)(currentInterval / 1000));
                         
            sensor_manager.update();
            _sensorReadingId++;
            yield(); // Ceder CPU al servidor web async entre operaciones pesadas
            
            // Acceso directo al sensor (sin JSON serialize/parse roundtrip)
            if (_liquidSensor) {
                float dist = _liquidSensor->getDistance();
                float pct = _liquidSensor->getPercentage();
                String estado = _liquidSensor->getLastError();
                float lit = _liquidSensor->getLitros();

                float currentLevel = lit;
                
                // Guardar lectura en historial circular
                lastReadings[readingIndex] = currentLevel;
                readingIndex = (readingIndex + 1) % 10;
                
                // Detección de estado de flujo (llenando/vaciando/idle)
                // NO detectar flujo si el sensor está inestable (lecturas multipath)
                bool wasFillingBefore = isFillingDetected;
                bool wasDrainingBefore = isDrainingDetected;
                bool sensorEstableNow = _liquidSensor ? _liquidSensor->isSensorStable() : true;
                
                FlowState flowState = FLOW_IDLE;
                if (sensorConfig.getAdaptiveInterval() && sensorEstableNow && !_installationMode) {
                    flowState = detectFlowState(lastReadings, sensorConfig.getFillingThreshold(), readingIndex);
                } else if (isFillingDetected || isDrainingDetected) {
                    // Sensor inestable: cancelar cualquier estado de flujo activo
                    isFillingDetected = false;
                    isDrainingDetected = false;
                    Serial.println("[FLUJO] Sensor inestable - cancelando deteccion de flujo");
                }
                
                // Modo forzado: activar llenado, pero auto-desactivar tras timeout si no hay llenado real
                if (_forceFillingMode) {
                    isFillingDetected = true;
                    isDrainingDetected = false;
                    unsigned long elapsed = millis() - _forceFillingTimestamp;
                    if (flowState == FLOW_FILLING) {
                        // Llenado real detectado: renovar el timeout
                        _forceFillingTimestamp = millis();
                    } else if (elapsed > FORCE_FILLING_TIMEOUT_MS) {
                        // Timeout: no se detectó llenado real en 2 min
                        _forceFillingMode = false;
                        isFillingDetected = false;
                        Serial.println("[LLENADO] Modo forzado EXPIRADO (sin llenado real)");
                        WebSerial.println("Modo llenado forzado EXPIRADO - sin llenado real detectado");
                    }
                } else {
                    isFillingDetected = (flowState == FLOW_FILLING);
                    isDrainingDetected = (flowState == FLOW_DRAINING);
                }
                
                if (isFillingDetected && !wasFillingBefore) {
                    Serial.printf("[FLUJO] LLENANDO detectado - intervalo: %ds\n", sensorConfig.getFillingInterval());
                    if (_liveMonitorEnabled) {
                        WebSerial.println("LLENANDO detectado - frecuencia aumentada");
                    }
                } else if (isDrainingDetected && !wasDrainingBefore) {
                    Serial.printf("[FLUJO] VACIANDO detectado - intervalo: %ds\n", sensorConfig.getDrainingInterval());
                    if (_liveMonitorEnabled) {
                        WebSerial.println("VACIANDO detectado - frecuencia aumentada");
                    }
                } else if (!isFillingDetected && !isDrainingDetected && (wasFillingBefore || wasDrainingBefore)) {
                    Serial.printf("[FLUJO] Estable - intervalo normal: %ds\n", sensorConfig.getNormalInterval());
                    if (_liveMonitorEnabled) {
                        WebSerial.println("Flujo estable - intervalo normal");
                    }
                }
                
                // Guardar en historial circular
                addSensorHistoryEntry(lit, dist, pct, estado);
                
                // Live monitor: enviar datos en tiempo real si está habilitado
                if (_liveMonitorEnabled) {
                    String ts = "";
                    if (ntpSync && ntpSync->isReady()) {
                        ts = ntpSync->getTimeWithMsString();
                    }
                    String line = "📡 ";
                    if (ts.length() > 0) line += "[" + ts + "] ";
                    line += String(lit, 1) + "L | " + String(dist, 1) + "cm | " + String(pct, 1) + "% | " + estado;
                    if (isFillingDetected) line += " [LLENANDO]";
                    if (isDrainingDetected) line += " [VACIANDO]";
                    line += " ◀ RESULTADO\n";
                    // Agregar detalles verbose de las mediciones
                    if (_liquidSensor) {
                        line += _liquidSensor->getLastMeasurementDetails();
                    }
                    WebSerial.println(line);
                }
                
                // Lectura local a las unidades emparejadas (unicast autenticado, con ACK).
                // Enviar funciona aunque la radio esté en modem sleep
                if (espNowManager && espNowManager->isInit() && espNowManager->hasPeers()) {
                    const ESPNowConfig& espCfg = config_manager.getESPNowConfig();
                    EspNowSensorPayload espData = {};
                    espData.sensorId = espCfg.getSensorID();
                    espData.flags = (_liquidSensor->hasValidReading() ? ESPNOW_FLAG_VALID : 0) |
                                    (isFillingDetected ? ESPNOW_FLAG_FILLING : 0) |
                                    (sensorEstableNow ? ESPNOW_FLAG_STABLE : 0);
                    espData.percent = pct;
                    espData.distance = dist;
                    espData.liters = lit;
                    espData.rssi = WiFi.status() == WL_CONNECTED ? (int8_t)WiFi.RSSI() : 0;
                    strncpy(espData.name, espNowManager->getLocalName().c_str(), sizeof(espData.name) - 1);
                    espNowManager->sendSensorToPeers(espData);
                }
            }
            
            yield(); // Ceder CPU antes de actualizar pantalla OLED
            
            // Actualizar pantalla con datos del sensor
            updateSensorDisplay();
            
            // Enviar datos a Home Assistant (si está conectado y WiFi activo)
            if (haMqtt && wifi_connected && haMqtt->isConnected()) {
                updateHAData(isFillingDetected);
            }
            
            // ESP-NOW: olvidar tinacos remotos sin lecturas (el TimeSync de antes mandaba
            // millis() como hora y nadie lo usaba: se quitó)
            if (espNowManager) {
                static unsigned long lastEspNowCleanup = 0;
                if (millis() - lastEspNowCleanup > 60000) {
                    espNowManager->clearOldSensors(ESPNOW_REMOTE_FRESH_MS);
                    lastEspNowCleanup = millis();
                }
            }
            
            lastSensorRead = millis();
        }
    }
    
    yield(); // Permitir que otras tareas del sistema se ejecuten
}

bool AppManager::isWiFiConnected() {
    return wifi_connected;
}

String AppManager::getDeviceIP() {
    if (wifi_connected && WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return WiFi.softAPIP().toString();
}

void AppManager::restart() {
    Serial.println("Reiniciando...");
    ESP.restart();
}

// ===== FUNCIONES DE MANEJO DE REQUESTS =====

void AppManager::handleScanWiFi(AsyncWebServerRequest *request) {
    Serial.println("🔍 Procesando solicitud de escaneo WiFi");
    
    // Verificar memoria disponible
    size_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 30000) { // Menos de 30KB libre
        Serial.printf("⚠️ Memoria insuficiente: %d bytes libres\n", freeHeap);
        String json = "[{\"error\":true,\"message\":\"Memoria insuficiente para escaneo\"}]";
        request->send(503, "application/json", json);
        return;
    }
    
    // Verificar intervalo de escaneo para evitar spam
    unsigned long now = millis();
    if (now - lastWiFiScan < WIFI_SCAN_INTERVAL) {
        Serial.println("⚠️ Escaneo muy frecuente, esperando...");
        String json = "[{\"waiting\":true,\"message\":\"Esperando intervalo de escaneo\"}]";
        request->send(429, "application/json", json);
        return;
    }
    lastWiFiScan = now;
    
    // Verificar si ya hay un escaneo en progreso
    int scanStatus = WiFi.scanComplete();
    if (scanStatus == WIFI_SCAN_RUNNING) {
        Serial.println("⚠️ Escaneo ya en progreso");
        String json = "[{\"scanning\":true,\"message\":\"Escaneo en progreso...\"}]";
        request->send(200, "application/json", json);
        return;
    }
    
    // Limpiar escaneo anterior si existe
    if (scanStatus >= 0) {
        WiFi.scanDelete();
    }
    
    Serial.printf("🔍 Iniciando escaneo WiFi (Heap libre: %d bytes)\n", ESP.getFreeHeap());
    
    // Alimentar watchdog antes del escaneo
    esp_task_wdt_reset();
    
    // Verificar que estamos en modo AP_STA (necesario para escanear)
    wifi_mode_t currentMode = WiFi.getMode();
    if (currentMode != WIFI_AP_STA) {
        Serial.printf("⚠️ Modo WiFi incorrecto para escaneo: %d, cambiando a AP_STA\n", currentMode);
        WiFi.mode(WIFI_AP_STA);
        delay(100);
    }
    
    // Escaneo asíncrono con configuración conservativa
    int result = WiFi.scanNetworks(true, false, false, 300); // async=true, max_ms_per_chan=300ms
    Serial.printf("🔍 WiFi.scanNetworks() resultado: %d (modo: %d)\n", result, WiFi.getMode());
    
    if (result == WIFI_SCAN_RUNNING) {
        String json = "[{\"scanning\":true,\"message\":\"Escaneo iniciado correctamente\"}]";
        request->send(200, "application/json", json);
    } else {
        Serial.printf("❌ Error iniciando escaneo: %d\n", result);
        String json = "[{\"error\":true,\"message\":\"Error iniciando escaneo WiFi\"}]";
        request->send(500, "application/json", json);
    }
}

void AppManager::handleWiFiResults(AsyncWebServerRequest *request) {
    Serial.println("📡 Procesando solicitud de resultados WiFi");
    
    int scanResult = WiFi.scanComplete();
    Serial.printf("📡 WiFi.scanComplete() = %d (modo: %d)\n", scanResult, WiFi.getMode());
    
    if (scanResult == WIFI_SCAN_RUNNING) {
        String json = "[{\"scanning\":true,\"message\":\"Escaneo aún en progreso...\"}]";
        request->send(200, "application/json", json);
        return;
    }
    
    if (scanResult == WIFI_SCAN_FAILED || scanResult < 0) {
        // Verificar que estamos en AP_STA antes de reintentar
        if (WiFi.getMode() != WIFI_AP_STA) {
            Serial.printf("⚠️ Modo WiFi incorrecto: %d, corrigiendo a AP_STA\n", WiFi.getMode());
            WiFi.mode(WIFI_AP_STA);
            delay(100);
        }
        
        Serial.printf("⚠️ Escaneo WiFi no disponible (status: %d), reintentando...\n", scanResult);
        WiFi.scanNetworks(true, false, false, 300);
        String json = "[{\"scanning\":true,\"message\":\"Reintentando escaneo...\"}]";
        request->send(200, "application/json", json);
        return;
    }
    
    // Construir JSON con resultados
    String json = "[";
    int maxNetworks = min(scanResult, 15); // Limitar a 15 redes
    
    for (int i = 0; i < maxNetworks; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
        json += "\"encryption\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        json += "}";
    }
    json += "]";
    
    WiFi.scanDelete(); // Limpiar resultados
    request->send(200, "application/json", json);
    Serial.printf("✅ Enviados %d resultados de escaneo\n", maxNetworks);
}

void AppManager::handleSensorData(AsyncWebServerRequest *request) {
    // Obtener datos REALES del sensor - usar buffer fijo para evitar fragmentación heap
    char jsonBuf[448];

    if (_installationMode) {
        // Solo la distancia cruda: el sensor se está instalando, no hay cálculos
        float d = _installRawCm;
        int len = snprintf(jsonBuf, sizeof(jsonBuf),
            "{\"litros\":0,\"distancia\":%.1f,\"porcentaje\":0,\"estadoSensor\":\"%s\","
            "\"sensorEstable\":true,\"readingId\":%lu,",
            d < 0 ? 0.0f : d, d <= INSTALL_WAITING ? "Esperando" : (d < 0 ? "Sin eco" : "Instalacion"),
            (unsigned long)_sensorReadingId);
        if (ntpSync && ntpSync->isReady()) {
            String tm = ntpSync->getTimeWithMsString();
            len += snprintf(jsonBuf + len, sizeof(jsonBuf) - len, "\"time\":\"%s\",", tm.c_str());
        }
        snprintf(jsonBuf + len, sizeof(jsonBuf) - len,
            "\"pumpActive\":%s,\"installationMode\":true,\"displaySleeping\":%s,\"autoSleepEnabled\":%s,\"timestamp\":%lu}",
            _forceFillingMode ? "true" : "false",
            display_manager.isDisplaySleeping() ? "true" : "false",
            display_manager.isAutoSleepEnabled() ? "true" : "false",
            millis());
        request->send(200, "application/json", jsonBuf);
        return;
    }

    if (_liquidSensor && _liquidSensor->hasValidReading()) {
        float distancia = _liquidSensor->getDistance();
        float porcentaje = _liquidSensor->getPercentage();
        float litros = _liquidSensor->getLitros();

        bool estable = _liquidSensor->isSensorStable();
        const char* estadoSensor = _liquidSensor->isFailsafeActive() ? "Lleno (seguridad)" 
                                 : (!estable ? "Inestable" : "Conectado");
        
        int len = snprintf(jsonBuf, sizeof(jsonBuf),
            "{\"litros\":%.1f,\"distancia\":%.1f,\"porcentaje\":%d,\"estadoSensor\":\"%s\","
            "\"sensorEstable\":%s,\"readingId\":%lu,",
            litros, distancia, (int)porcentaje, estadoSensor,
            estable ? "true" : "false", (unsigned long)_sensorReadingId);
        
        if (ntpSync && ntpSync->isReady()) {
            String tm = ntpSync->getTimeWithMsString();
            len += snprintf(jsonBuf + len, sizeof(jsonBuf) - len, "\"time\":\"%s\",", tm.c_str());
        }
        
        snprintf(jsonBuf + len, sizeof(jsonBuf) - len,
            "\"pumpActive\":%s,\"installationMode\":%s,\"displaySleeping\":%s,\"autoSleepEnabled\":%s,\"timestamp\":%lu}",
            _forceFillingMode ? "true" : "false",
            _installationMode ? "true" : "false",
            display_manager.isDisplaySleeping() ? "true" : "false",
            display_manager.isAutoSleepEnabled() ? "true" : "false",
            millis());
    } else {
        int len = snprintf(jsonBuf, sizeof(jsonBuf),
            "{\"litros\":0,\"distancia\":0,\"porcentaje\":0,\"estadoSensor\":\"Error\","
            "\"sensorEstable\":false,\"readingId\":%lu,",
            (unsigned long)_sensorReadingId);
        
        if (ntpSync && ntpSync->isReady()) {
            String tm = ntpSync->getTimeWithMsString();
            len += snprintf(jsonBuf + len, sizeof(jsonBuf) - len, "\"time\":\"%s\",", tm.c_str());
        }
        
        snprintf(jsonBuf + len, sizeof(jsonBuf) - len,
            "\"pumpActive\":%s,\"installationMode\":%s,\"displaySleeping\":%s,\"autoSleepEnabled\":%s,\"timestamp\":%lu}",
            _forceFillingMode ? "true" : "false",
            _installationMode ? "true" : "false",
            display_manager.isDisplaySleeping() ? "true" : "false",
            display_manager.isAutoSleepEnabled() ? "true" : "false",
            millis());
    }
    
    request->send(200, "application/json", jsonBuf);
}

void AppManager::handleToggleDisplay(AsyncWebServerRequest *request) {
    // NO llamar sensor_manager.update() aquí — bloquea AsyncTCP 60ms
    if (display_manager.isDisplaySleeping()) {
        display_manager.wakeUpDisplay();
        updateSensorDisplay(true); // Usar última lectura en RAM
        request->send(200, "application/json", "{\"display\":\"awake\",\"message\":\"Pantalla encendida\"}");
    } else {
        display_manager.forceDisplaySleep();
        request->send(200, "application/json", "{\"display\":\"sleeping\",\"message\":\"Pantalla apagada\"}");
    }
}

void AppManager::updateSensorDisplay(bool forceRedraw) {
    // Solo actualizar pantalla si no está durmiendo y estamos en modo sensor
    if (portal_active || display_manager.isDisplaySleeping()) {
        return;
    }
    
    // Variables estáticas para almacenar valores anteriores
    static float lastDistance = -999.0;
    static float lastLitros = -999.0;
    static int lastPercentage = -999;
    static String lastStatus = "";
    static bool firstRun = true;
    
    // Forzar redibujado completo si se solicita (ej: después de despertar pantalla)
    if (forceRedraw) {
        firstRun = true;
        lastDistance = -999.0;
        lastLitros = -999.0;
        lastPercentage = -999;
        lastStatus = "";
    }
    
    // Acceso directo al sensor (sin JSON serialize/parse roundtrip)
    bool sensorValid = _liquidSensor && _liquidSensor->hasValidReading();
    
    // Si es primera ejecución o sensor inválido, redibujar todo
    if (firstRun || !sensorValid) {
        firstRun = false;
        
        display_manager.clear();
        display_manager.setFont(ArialMT_Plain_10);
        display_manager.setTextAlignment(TEXT_ALIGN_LEFT);
        
        // Línea 1: Título con rol ESP-NOW mientras está encendido (E = emisor, R = receptor)
        String title = "SENSOR NIVEL LIQUIDO";
        if (espNowManager && espNowManager->isInit()) {
            title += config_manager.getESPNowConfig().isReceptor() ? " [R]" : " [E]";
        }
        display_manager.drawString(0, 0, title);

        if (!sensorValid) {
            // Mostrar error del sensor
            display_manager.drawString(0, 12, "SENSOR ERROR");
            display_manager.drawString(0, 24, "Verificar conexion");
            display_manager.drawString(0, 36, "Trig/Echo pins");

            // Mostrar info de red (o el tinaco remoto) en caso de error
            String remote = currentRemoteLine();
            if (remote.length() > 0) {
                display_manager.drawString(0, 48, remote);
            } else if (WiFi.status() == WL_CONNECTED) {
                display_manager.drawString(0, 48, "WiFi: " + WiFi.SSID());
            } else {
                display_manager.drawString(0, 48, "Sin WiFi");
            }
            
            display_manager.display();
            
            // Resetear valores para próxima actualización
            lastDistance = -999.0;
            lastLitros = -999.0;
            lastPercentage = -999;
            lastStatus = "";
            return;
        }
        
        // Dibujar etiquetas estáticas
        display_manager.drawString(0, 12, "Dist:");
        display_manager.drawString(0, 24, "Liq:");
        display_manager.drawString(0, 36, "Lleno:");
    }
    
    // Obtener valores directamente del sensor (sin JSON roundtrip)
    float distance = _liquidSensor->getDistance();
    int percentage = (int)_liquidSensor->getPercentage();
    float litros = _liquidSensor->getLitros();

    // Rastrear si hubo cambios para enviar buffer al OLED
    bool needsDisplay = false;
    
    // Solo actualizar si cambió la distancia
    if (distance != lastDistance) {
        // Borrar solo el área del valor anterior (aprox 60 pixels)
        display_manager.setColor(0);  // BLACK
        display_manager.fillRect(36, 12, 92, 10, 0);
        display_manager.setColor(1);  // WHITE
        
        // Dibujar nuevo valor
        display_manager.drawString(36, 12, String(distance, 1) + " cm");
        lastDistance = distance;
        needsDisplay = true;
    }
    
    // Solo actualizar si cambió litros
    if (litros != lastLitros) {
        // Borrar solo el área del valor anterior
        display_manager.setColor(0);
        display_manager.fillRect(36, 24, 92, 10, 0);
        display_manager.setColor(1);
        
        // Dibujar nuevo valor
        display_manager.drawString(36, 24, String(litros, 1) + " L");
        lastLitros = litros;
        needsDisplay = true;
    }
    
    // Solo actualizar si cambió porcentaje
    if (percentage != lastPercentage) {
        // Borrar solo el área del valor anterior
        display_manager.setColor(0);
        display_manager.fillRect(42, 36, 86, 10, 0);
        display_manager.setColor(1);
        
        // Dibujar nuevo valor
        display_manager.drawString(42, 36, String(percentage) + " %");
        lastPercentage = percentage;
        needsDisplay = true;
    }
    
    // Línea de estado (WiFi/fecha)
    String statusLine = "";
    if (ntpSync && ntpSync->isReady() && config_manager.getSensorConfig().getShowDateTime()) {
        statusLine = ntpSync->getCompactDateTime(); // "27/09 14:30"
    } else if (WiFi.status() == WL_CONNECTED) {
        statusLine = "WiFi: OK";
    } else {
        statusLine = "Sin WiFi";
    }
    // Receptor ESP-NOW: alterna con los tinacos remotos (cada 4 s)
    String remoteLine = currentRemoteLine();
    if (remoteLine.length() > 0) statusLine = remoteLine;

    // Solo actualizar línea de estado si cambió
    if (statusLine != lastStatus) {
        display_manager.setColor(0);
        display_manager.fillRect(0, 48, 128, 16, 0);
        display_manager.setColor(1);
        
        display_manager.drawString(0, 48, statusLine);
        lastStatus = statusLine;
        needsDisplay = true;
    }
    
    // Enviar buffer al OLED si hubo cambios
    if (needsDisplay) {
        display_manager.display();
    }
}

// ===========================================================
// PANTALLA OLED EN MODO INSTALACIÓN
// Muestra solo la distancia en formato grande para facilitar
// la colocación física del sensor sobre el contenedor.
// ===========================================================
void AppManager::updateInstallationDisplay(float distancia) {
    if (display_manager.isDisplaySleeping()) return;
    
    display_manager.clear();
    
    // Título
    display_manager.setFont(ArialMT_Plain_10);
    display_manager.setTextAlignment(TEXT_ALIGN_CENTER);
    display_manager.drawString(64, 0, "MODO INSTALACION");
    
    // Distancia grande
    display_manager.setFont(ArialMT_Plain_24);
    if (distancia >= 0) {
        display_manager.drawString(64, 18, String(distancia, 1) + " cm");
    } else {
        display_manager.drawString(64, 18, "ERROR");
    }
    
    // Indicador de calidad
    display_manager.setFont(ArialMT_Plain_10);
    if (distancia >= 0 && distancia <= 400) {
        if (distancia < 2) {
            display_manager.drawString(64, 48, "Muy cerca (< 2cm)");
        } else if (distancia > 300) {
            display_manager.drawString(64, 48, "Muy lejos (> 3m)");
        } else {
            display_manager.drawString(64, 48, "Rango OK");
        }
    } else {
        display_manager.drawString(64, 48, "Sin eco - revisar sensor");
    }
    
    display_manager.setTextAlignment(TEXT_ALIGN_LEFT); // Restaurar alineación
    display_manager.display();
}

void AppManager::updateHAData(bool isFilling) {
    if (!haMqtt || !haMqtt->isConnected()) return;
    
    // Obtener datos directamente del sensor (sin JSON roundtrip)
    float litros = 0, distancia = 0, porcentaje = 0;
    if (_liquidSensor && _liquidSensor->hasValidReading()) {
        distancia = _liquidSensor->getDistance();
        porcentaje = _liquidSensor->getPercentage();
        litros = _liquidSensor->getLitros();
    }
    
    bool alerta = (porcentaje < 20.0);
    bool sensorEstable = _liquidSensor ? _liquidSensor->isSensorStable() : true;
    
    // Publicar datos vía MQTT
    haMqtt->publishSensorData(litros, porcentaje, distancia, alerta, isFilling, sensorEstable, _installationMode);
}

void AppManager::handleHACommand(const String& topic, const String& payload) {
    Serial.printf("🏠 Comando HA recibido: topic=%s payload=%s\n", topic.c_str(), payload.c_str());
    
    // Comandos: "FILLING_ON" / "FILLING_OFF" (enviados por switch de HA "Bomba de Agua")
    String cmd = payload;
    cmd.trim();
    cmd.toUpperCase();
    
    if (cmd == "FILLING_ON" || cmd == "ON") {
        _forceFillingMode = true;
        _forceFillingTimestamp = millis();
        Serial.println("[HA] 💧 Bomba ON → modo llenado activado (intervalo rápido)");
        if (haMqtt) haMqtt->publishPumpState(true);
    } else if (cmd == "FILLING_OFF" || cmd == "OFF") {
        _forceFillingMode = false;
        Serial.println("[HA] 🛑 Bomba OFF → modo llenado desactivado (intervalo normal)");
        if (haMqtt) haMqtt->publishPumpState(false);
    } else {
        Serial.printf("[HA] ⚠️ Comando desconocido: %s\n", payload.c_str());
    }
}

bool AppManager::detectFilling(float readings[], uint8_t threshold, uint8_t currentIndex) {
    return detectFlowState(readings, threshold, currentIndex) == FLOW_FILLING;
}

FlowState AppManager::detectFlowState(float readings[], uint8_t threshold, uint8_t currentIndex) {
    // Algoritmo v3: detecta llenado Y vaciado
    // - Recorre el buffer circular en orden temporal correcto
    // - Detección rápida: delta acumulado sobre ventana corta (threshold lecturas)
    // - Detección lenta: delta acumulado sobre ventana larga (todas las lecturas)
    // - Misma lógica simétrica para subida (filling) y bajada (draining)
    
    if (threshold < 2 || threshold > 10) {
        threshold = 3;
    }
    
    // Obtener lecturas en orden temporal (más reciente primero)
    float ordered[10];
    uint8_t count = 0;
    for (uint8_t i = 0; i < 10; i++) {
        uint8_t idx = (currentIndex + 10 - 1 - i) % 10;
        if (readings[idx] > 0) {
            ordered[count++] = readings[idx];
        }
    }
    
    if (count < threshold) {
        return FLOW_IDLE;
    }
    
    // === Detección rápida (flujo alto): ventana corta ===
    uint8_t shortWindow = (count < threshold) ? count : threshold;
    float shortDelta = ordered[0] - ordered[shortWindow - 1]; // positivo = llenando, negativo = vaciando
    float minShortDelta = 5.0 * (shortWindow - 1); // 10L mínimo para 3 lecturas (evita falsos positivos por ruido)
    
    bool fastFilling = false;
    bool fastDraining = false;
    
    if (shortDelta >= minShortDelta) {
        uint8_t rising = 0;
        for (uint8_t i = 0; i < shortWindow - 1; i++) {
            if (ordered[i] >= ordered[i + 1]) rising++;
        }
        fastFilling = (rising >= (shortWindow - 1 + 1) / 2);
    } else if (shortDelta <= -minShortDelta) {
        uint8_t falling = 0;
        for (uint8_t i = 0; i < shortWindow - 1; i++) {
            if (ordered[i] <= ordered[i + 1]) falling++;
        }
        fastDraining = (falling >= (shortWindow - 1 + 1) / 2);
    }
    
    // === Detección lenta (flujo bajo): ventana larga ===
    bool slowFilling = false;
    bool slowDraining = false;
    if (count >= 5) {
        float longDelta = ordered[0] - ordered[count - 1];
        float minLongDelta = 15.0; // Mínimo 15L de cambio total (evita falsas detecciones por ruido sensor)
        
        if (longDelta >= minLongDelta) {
            uint8_t rising = 0;
            for (uint8_t i = 0; i < count - 1; i++) {
                if (ordered[i] >= ordered[i + 1]) rising++;
            }
            slowFilling = (rising > (count - 1) / 2);
        } else if (longDelta <= -minLongDelta) {
            uint8_t falling = 0;
            for (uint8_t i = 0; i < count - 1; i++) {
                if (ordered[i] <= ordered[i + 1]) falling++;
            }
            slowDraining = (falling > (count - 1) / 2);
        }
    }
    
    bool isIncreasing = fastFilling || slowFilling;
    bool isDecreasing = fastDraining || slowDraining;
    
    if (isIncreasing) {
        float totalDelta = ordered[0] - ordered[count - 1];
        Serial.printf("[FLUJO] LLENANDO (%s): +%.1fL, lecturas=%d\n",
                     fastFilling ? "rapido" : "lento", totalDelta, count);
        return FLOW_FILLING;
    }
    
    if (isDecreasing) {
        float totalDelta = ordered[count - 1] - ordered[0];
        Serial.printf("[FLUJO] VACIANDO (%s): -%.1fL, lecturas=%d\n",
                     fastDraining ? "rapido" : "lento", totalDelta, count);
        return FLOW_DRAINING;
    }
    
    return FLOW_IDLE;
}

// ============================================================
// HISTORIAL CIRCULAR DE SENSOR - almacena últimas N lecturas
// ============================================================
void AppManager::addSensorHistoryEntry(float litros, float distancia, float porcentaje, const String& estado) {
    SensorHistoryEntry& entry = _sensorHistory[_historyIndex];
    entry.litros = litros;
    entry.distancia = distancia;
    entry.porcentaje = porcentaje;
    entry.timestamp = (ntpSync && ntpSync->isReady()) ? ntpSync->getCurrentTimestamp() : 0;
    strncpy(entry.estado, estado.c_str(), sizeof(entry.estado) - 1);
    entry.estado[sizeof(entry.estado) - 1] = '\0';
    
    _historyIndex = (_historyIndex + 1) % HISTORY_SIZE;
    if (_historyCount < HISTORY_SIZE) _historyCount++;
}

// ============================================================
// FLAG DE FORZAR MODO NORMAL TRAS REINICIO
// Se escribe antes de ESP.restart() desde /api/save-and-restart
// para garantizar que el próximo arranque intente WiFi sin importar DRD
// ============================================================
void AppManager::setForceNormalBoot() {
    File f = LittleFS.open("/force_normal.flag", "w");
    if (f) {
        f.print("1");
        f.close();
        Serial.println("🟢 Flag de modo normal escrito en /force_normal.flag");
    } else {
        Serial.println("⚠️ Error escribiendo flag de modo normal");
    }
}

bool AppManager::checkForceNormalBoot() {
    if (LittleFS.exists("/force_normal.flag")) {
        LittleFS.remove("/force_normal.flag");
        Serial.println("🟢 Flag de modo normal encontrado y eliminado");
        return true;
    }
    return false;
}

void AppManager::setForcePortalBoot() {
    File f = LittleFS.open("/force_portal.flag", "w");
    if (f) {
        f.print("1");
        f.close();
        Serial.println("📡 Flag de portal cautivo escrito en /force_portal.flag");
    } else {
        Serial.println("⚠️ Error escribiendo flag de portal cautivo");
    }
}

bool AppManager::checkForcePortalBoot() {
    if (LittleFS.exists("/force_portal.flag")) {
        LittleFS.remove("/force_portal.flag");
        Serial.println("📡 Flag de portal cautivo encontrado y eliminado");
        return true;
    }
    return false;
}

// ============================================================
// CONEXIÓN A UNA RED WIFI ESPECÍFICA
// ============================================================
bool AppManager::tryConnectToNetwork(const WiFiNetwork& network) {
    Serial.printf("Conectando a: '%s'\n", network.ssid.c_str());
    
    // Mostrar en OLED qué red se está intentando
    display_manager.clear();
    display_manager.setFont(ArialMT_Plain_10);
    display_manager.setTextAlignment(TEXT_ALIGN_CENTER);
    display_manager.drawString(64, 5, "Conectando WiFi...");
    display_manager.drawString(64, 20, network.ssid.c_str());
    display_manager.display();
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(100);
    
    // Aplicar hostname
    String hostname = config_manager.getHostname();
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    String macSuffix = mac.substring(mac.length() - 4);
    macSuffix.toLowerCase();
    if (hostname == "sensor-nivel-liquido" || hostname == "TinacoESP") {
        hostname = "sensor-nivel-liquido-" + macSuffix;
    }
    WiFi.setHostname(hostname.c_str());
    
    WiFi.begin(network.ssid.c_str(), network.password.c_str());
    
    int attempts = 0;
    const int maxAttempts = 20; // 10s max (20 × 500ms)
    while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
        delay(500);
        Serial.print(".");
        attempts++;
        esp_task_wdt_reset();
        
        // Actualizar barra de progreso en OLED cada 2 intentos
        if (attempts % 2 == 0) {
            display_manager.clear();
            display_manager.setFont(ArialMT_Plain_10);
            display_manager.setTextAlignment(TEXT_ALIGN_CENTER);
            display_manager.drawString(64, 5, "Conectando WiFi...");
            display_manager.drawString(64, 20, network.ssid.c_str());
            // Barra de progreso
            int barWidth = (attempts * 100) / maxAttempts;
            display_manager.drawProgressBar(14, 38, 100, 10, barWidth);
            display_manager.display();
        }
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WiFi conectado a '%s' IP: %s\n", network.ssid.c_str(), WiFi.localIP().toString().c_str());
        return true;
    } else {
        Serial.printf("Fallo conectar a '%s' (estado: %d)\n", network.ssid.c_str(), WiFi.status(), attempts);
        return false;
    }
}