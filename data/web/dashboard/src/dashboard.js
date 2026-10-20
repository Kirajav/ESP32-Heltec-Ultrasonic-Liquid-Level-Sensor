let websocket;
let isConnected = false;
let currentData = {};
let espNowSensors = [];
let animationInterval;
let containerType = 'tank'; // 'tank', 'container', 'cistern'
let containerSVGs = {};
let lastErrorToast = 0; // Rate limit para toasts de error
let consecutiveErrors = 0; // Contador de errores consecutivos
let networkStatus = {
connected: false,
deviceCount: 0,
lastUpdate: null
};
let tankCapacity = 0;
window.addEventListener("load", () => {
    console.log("🚀 Iniciando Smart Liquid Sensor Dashboard Unificado...");

    // Preparar SVGs para tipos de contenedor (NO reemplazar el SVG del HTML)
    initContainerSVGs();
    initAnimations();
    updateMainSensorTitle();

    // Conectar al ESP32 y obtener datos reales
    startPolling();

    // Inicializar botones y otras funcionalidades (sin fetches)
    initButtons();
    initSensorLog();
    setupSensorConfig();
    setupMQTTConfig();

    // Escalonar cargas para no sobrecargar el ESP32
    // 2s: Historial de sensor + estado display
    setTimeout(() => {
        loadSensorHistory();
        loadDisplayStatus();
    }, 2000);

    // 4s: Config del sensor (contenedor + specs en una sola llamada)
    setTimeout(() => {
        loadInitialConfig();
    }, 4000);

    // 8s: ESP-NOW (no crítico, cargar después)
    setTimeout(() => {
        setupMultiSensorButton();
        loadESPNowSensors();
        startESPNowPolling();
        updateNetworkStatus();
    }, 8000);

    console.log("✅ Dashboard inicializado con datos en tiempo real.");
});
// WebSocket removido - se usa polling HTTP exclusivamente
// Las funciones initWebSocket, onOpen, onClose, onError, onMessage
// han sido reemplazadas por polling con manejo de estado de conexión
function updateConnectionStatus(connected) {
const statusDot = document.getElementById('sensor-status-dot');
const statusText = document.getElementById('valor-estado-sensor');
if (!statusDot || !statusText) return;
if (connected) {
statusDot.style.background = '#4CAF50';
statusText.textContent = 'Conectado';
statusText.style.color = '#4CAF50';
} else {
statusDot.style.background = '#F44336';
statusText.textContent = 'Desconectado';
statusText.style.color = '#F44336';
}
}
function loadDisplayStatus() {
fetch('/api/status')
.then(response => response.json())
.then(data => {
console.log("📱 Estado display cargado:", data);
updateDisplayStatus(data.status);
})
.catch(error => {
console.error("❌ Error cargando estado display:", error);
updateDisplayStatus("Desconocido");
});
}
function loadTechSpecs() {
// NOTA: Esta función ya no se usa directamente.
// Se mantiene por compatibilidad pero loadInitialConfig() la reemplaza.
}
// Carga unificada de /api/sensor/config: contenedor + specs en una sola llamada
function loadInitialConfig() {
console.log("⚙️ Cargando configuración inicial (contenedor + specs)...");
const controller = new AbortController();
setTimeout(() => controller.abort(), 8000);
fetch('/api/sensor/config', { signal: controller.signal })
.then(response => {
if (!response.ok) throw new Error('HTTP ' + response.status);
return response.json();
})
.then(config => {
if (!config || !config.success) return;
// --- Parte 1: Tipo de contenedor (antes en loadAndSetContainerType) ---
if (typeof config.container_type !== 'undefined') {
const typeMap = { 0: 'tank', 1: 'cistern', 2: 'container' };
const newType = typeMap[config.container_type] || 'tank';
console.log("✅ Tipo de contenedor: " + newType);
if (newType !== containerType) {
containerType = newType;
localStorage.setItem('containerType', containerType);
renderContainerSVG();
}
updateMainSensorTitle();
if (config.tank_capacity && config.tank_capacity > 0) {
tankCapacity = config.tank_capacity;
updateLitersScale(tankCapacity);
console.log("📏 Capacidad del tanque: " + tankCapacity + "L");
}
}
// --- Parte 2: Specs técnicas (antes en loadTechSpecs) ---
const note = document.getElementById('sensor-info-note');
if (note) {
    const name = config.sensor_model_name || 'Sensor';
    const minD = config.min_distance || 2;
    note.innerHTML = '<strong>Sensor ' + name + ':</strong> Distancia m\u00ednima fiable ' + minD + 'cm. Lecturas menores se interpretan como tanque lleno.';
}
const rango = document.getElementById('spec-rango');
if (rango) rango.textContent = (config.min_distance || 2) + '-' + (config.max_distance || 400) + ' cm';
const intervalo = document.getElementById('spec-intervalo');
if (intervalo) intervalo.textContent = (config.normal_interval || 30) + ' seg';
// Guardar intervalo para throttle del log
addSensorLogEntry._minInterval = config.normal_interval || 30;
console.log("✅ Configuración inicial cargada (log interval: " + addSensorLogEntry._minInterval + "s)");
})
.catch(error => {
console.warn("⚠️ Error cargando config:", error.message);
updateMainSensorTitle();
});
}
function updateDisplayStatus(status) {
const statusElement = document.getElementById('valor-estado-oled');
if (statusElement) {
statusElement.textContent = status;
console.log("📱 Display actualizado:", status);
}
}
function startPolling() {
console.log("🔄 Iniciando polling de datos...");
let pollingActive = false;
const poll = async () => {
if (pollingActive) return; // Evitar polls simultáneos
pollingActive = true;
try {
const controller = new AbortController();
const timeoutId = setTimeout(() => controller.abort(), 8000);
const response = await fetch('/api/sensor-data', { signal: controller.signal });
clearTimeout(timeoutId);
if (!response.ok) {
throw new Error('HTTP ' + response.status);
}
const data = await response.json();
currentData = data;
updateConnectionStatus(true);
consecutiveErrors = 0;
updateSensorData(data);
addSensorLogEntry(data);
checkPumpStateChange(data);
try {
updateContainerAnimation(data);
} catch (animError) {
console.error("❌ Error en animación:", animError);
}
} catch (error) {
console.error("❌ Error en polling:", error.message);
consecutiveErrors++;
if (consecutiveErrors >= 5) {
updateConnectionStatus(false);
const now = Date.now();
if (now - lastErrorToast > 60000) {
showToast("Error obteniendo datos del sensor", "warning");
lastErrorToast = now;
}
}
} finally {
pollingActive = false;
}
};
setTimeout(poll, 1500); // Primer poll con 1.5s de delay
setInterval(poll, 5000); // Polling cada 5 segundos
}
function updateSensorData(data) {
console.log("📊 Actualizando datos:", data);
updateElement('Litros', data.litros);
updateElement('Distancia', data.distancia);
// Usar porcentaje del servidor si existe, sino calcularlo del cliente
const porcentaje = (data.porcentaje !== undefined && data.porcentaje !== null) 
    ? data.porcentaje 
    : calculatePercentage(data.distancia);
data.Porcentaje = porcentaje; // Normalizar para animaciones de contenedor
updateElement('Porcentaje', porcentaje);
updateElement('valor-estado-sensor', data.estadoSensor || 'Conectado');
// Reflejar estado del modo instalación en el UI
const modoLabel = document.getElementById('valor-modo-herramienta');
const installBtn = document.getElementById('button-install-mode');
if (data.installationMode) {
    if (modoLabel) modoLabel.textContent = 'Instalacion';
    if (installBtn) {
        installBtn.textContent = 'Salir Modo Instalacion';
        installBtn.className = 'modern-btn secondary';
    }
    updateElement('valor-estado-sensor', 'Modo Instalacion');
} else {
    if (modoLabel && modoLabel.textContent === 'Instalacion') {
        modoLabel.textContent = 'Normal';
    }
    if (installBtn && installBtn.textContent !== 'Modo Instalacion') {
        installBtn.textContent = 'Modo Instalacion';
        installBtn.className = 'modern-btn primary';
    }
}
// Actualizar estado del display OLED
if (data.displaySleeping !== undefined) {
    const oledStatus = document.getElementById('valor-estado-oled');
    if (oledStatus) oledStatus.textContent = data.displaySleeping ? 'Apagado' : 'Encendido';
}
// Sincronizar estado de auto-sleep
if (data.autoSleepEnabled !== undefined) {
    updateAutoSleepButton(data.autoSleepEnabled);
}
}
function updateElement(id, value) {
const element = document.getElementById(id);
if (element) {
element.textContent = (value !== null && value !== undefined && value !== '') ? value : '--';
}
}
function calculatePercentage(distancia) {
if (distancia === null || distancia === undefined || distancia === '' || distancia === '--') return '--';
const dist = parseFloat(distancia);
if (isNaN(dist)) return '--';
const minDist = 19;
const maxDist = 200;
let percentage;
if (dist <= minDist) {
percentage = 100;
} else if (dist >= maxDist) {
percentage = 0;
} else {
percentage = Math.round(((maxDist - dist) / (maxDist - minDist)) * 100);
}
return Math.max(0, Math.min(100, percentage));
}
function initAnimations() {
console.log("🎨 Inicializando animaciones SVG...");
startWaveAnimation();
}
// updateTankAnimation is defined in the container animation section below
function animateLiquidLevel(percentage) {
const liquidLevel = getSVGElement('liquid-level');
const liquidWaves = getSVGElement('liquid-waves');
if (liquidLevel && liquidWaves) {
const maxHeight = 270;
const height = (percentage / 100) * maxHeight;
const yPosition = 350 - height;
liquidLevel.style.transition = 'all 1.5s ease-in-out';
liquidLevel.setAttribute('y', yPosition);
liquidLevel.setAttribute('height', height);
liquidWaves.style.transition = 'all 1.5s ease-in-out';
liquidWaves.setAttribute('y', yPosition);
liquidWaves.setAttribute('height', height);
console.log(`🌊 Nivel animado: ${percentage}% (altura: ${height}px)`);
}
}
function animateLevelBar(percentage) {
const levelBar = getSVGElement('level-bar');
if (levelBar) {
const maxHeight = 270;
const height = (percentage / 100) * maxHeight;
const yPosition = 348 - height;
levelBar.style.transition = 'all 1.5s ease-in-out';
levelBar.setAttribute('y', yPosition);
levelBar.setAttribute('height', height);
}
}
function updateLiquidColor(percentage) {
const liquidLevel = getSVGElement('liquid-level');
if (liquidLevel) {
let color1, color2, color3;
if (percentage >= 75) {
color1 = '#4CAF50'; color2 = '#66BB6A'; color3 = '#2E7D32';
} else if (percentage >= 50) {
color1 = '#4FC3F7'; color2 = '#29B6F6'; color3 = '#0288D1';
} else if (percentage >= 25) {
color1 = '#FFB74D'; color2 = '#FF9800'; color3 = '#F57C00';
} else {
color1 = '#EF5350'; color2 = '#F44336'; color3 = '#C62828';
}
const gradient = getSVGElement('liquidGradient');
if (gradient) {
const stops = gradient.querySelectorAll('stop');
if (stops.length >= 3) {
stops[0].setAttribute('style', `stop-color:${color1};stop-opacity:0.9`);
stops[1].setAttribute('style', `stop-color:${color2};stop-opacity:0.8`);
stops[2].setAttribute('style', `stop-color:${color3};stop-opacity:1`);
}
}
}
}
function startWaveAnimation() {
let offset = 0;
setInterval(() => {
const wavePattern = getSVGElement('wavePattern');
if (wavePattern) {
offset += 2;
if (offset >= 100) offset = 0;
wavePattern.setAttribute('x', offset);
}
}, 100);
}
function initButtons() {
console.log("🔘 Inicializando botones...");
const toggleButton = document.getElementById('button-toggle');
const resetButton = document.getElementById('button-reset');
const wifiButton = document.getElementById('wifi-config-button');
const installBtn = document.getElementById('button-install-mode');
const calibrateBtn = document.getElementById('button-calibrate');
if (toggleButton) {
toggleButton.addEventListener('click', toggleDisplay);
}
var autoSleepBtn = document.getElementById('button-auto-sleep');
if (autoSleepBtn) {
autoSleepBtn.addEventListener('click', toggleAutoSleep);
}
if (resetButton) {
resetButton.addEventListener('click', resetESP32);
}
if (wifiButton) {
wifiButton.addEventListener('click', resetWiFiConfig);
}
if (installBtn) {
installBtn.addEventListener('click', toggleInstallationMode);
}
if (calibrateBtn) {
calibrateBtn.addEventListener('click', calibrateBottom);
}
// Inicializar dropdown con soporte táctil/click
initDropdown();
}
function initDropdown() {
const dropdown = document.querySelector('.dropdown');
const toggleBtn = document.querySelector('.dropdown-toggle');
const menu = document.querySelector('.dropdown-menu');
if (!dropdown || !toggleBtn || !menu) return;
// Toggle dropdown al hacer click en el botón
toggleBtn.addEventListener('click', function(e) {
e.preventDefault();
e.stopPropagation();
dropdown.classList.toggle('active');
});
// Cerrar dropdown al hacer click fuera
document.addEventListener('click', function(e) {
if (!dropdown.contains(e.target)) {
dropdown.classList.remove('active');
}
});
// Cerrar dropdown al seleccionar una opción (excepto links reales)
menu.querySelectorAll('a[href="#"]').forEach(function(link) {
link.addEventListener('click', function() {
setTimeout(function() { dropdown.classList.remove('active'); }, 150);
});
});
console.log("✅ Dropdown inicializado con soporte táctil");
}
function toggleDisplay() {
console.log("🔄 Alternando display...");
showToast("Alternando display OLED...", "info");
fetch('/api/toggle-display', { method: 'POST' })
.then(response => response.json())
.then(data => {
console.log("✅ Display alternado:", data);
const oledStatus = document.getElementById('valor-estado-oled');
if (oledStatus) {
    oledStatus.textContent = data.display === 'awake' ? 'Encendido' : 'Apagado';
}
showToast(data.message || "Display alternado correctamente", "success");
})
.catch(error => {
console.error("❌ Error alternando display:", error);
showToast("Error alternando display", "error");
});
}
function toggleAutoSleep() {
console.log("⏰ Alternando auto-sleep...");
fetch('/api/auto-sleep', { method: 'POST' })
.then(function(r) { return r.json(); })
.then(function(data) {
console.log("✅ Auto-sleep:", data);
updateAutoSleepButton(data.autoSleepEnabled);
showToast(data.message || "Auto-sleep actualizado", "success");
// Si se desactivó y el display estaba dormido, actualizar estado
if (!data.autoSleepEnabled) {
    var oledStatus = document.getElementById('valor-estado-oled');
    if (oledStatus && oledStatus.textContent === 'Apagado') {
        oledStatus.textContent = 'Encendido';
    }
}
})
.catch(function(error) {
console.error("❌ Error auto-sleep:", error);
showToast("Error cambiando auto-sleep", "error");
});
}
function updateAutoSleepButton(enabled) {
var btn = document.getElementById('button-auto-sleep');
if (!btn) return;
if (enabled) {
    btn.innerHTML = '<span class="material-symbols-outlined">bedtime</span> Deshabilitar Apagado Auto';
    btn.className = 'modern-btn secondary';
} else {
    btn.innerHTML = '<span class="material-symbols-outlined">bedtime_off</span> Habilitar Apagado Auto';
    btn.className = 'modern-btn primary';
}
}
function resetESP32() {
if (confirm("⚠️ ¿Estás seguro de que quieres reiniciar el ESP32?")) {
console.log("🔄 Reiniciando ESP32...");
showToast("Reiniciando ESP32...", "warning");
fetch('/api/restart', { method: 'POST' })
.then(() => {
showToast("ESP32 reiniciado. Reconectando...", "info");
setTimeout(() => location.reload(), 3000);
})
.catch(error => {
console.error("❌ Error reiniciando:", error);
showToast("Error reiniciando ESP32", "error");
});
}
}
function resetWiFiConfig() {
if (confirm("⚠️ ¿Reiniciar configuración WiFi?\nSe borrarán las redes guardadas y el dispositivo entrará en modo portal cautivo para configurar una nueva red.")) {
console.log("📶 Reiniciando configuración WiFi...");
showToast("Borrando redes WiFi y reiniciando...", "warning");
fetch('/api/reset-wifi', { method: 'POST' })
.then(r => r.json())
.then(data => {
showToast(data.message || "WiFi reiniciado. Conéctate al AP para reconfigurar.", "info");
})
.catch(error => {
console.error("❌ Error reiniciando WiFi:", error);
showToast("Error reiniciando WiFi", "error");
});
}
}
function toggleInstallationMode() {
const btn = document.getElementById('button-install-mode');
if (btn) btn.disabled = true;
showToast("Cambiando modo...", "info");
fetch('/api/installation-mode', { method: 'POST' })
.then(r => r.json())
.then(data => {
const active = data.installationMode;
const label = document.getElementById('valor-modo-herramienta');
if (label) label.textContent = active ? 'Instalacion' : 'Normal';
if (btn) {
btn.textContent = active ? 'Salir Modo Instalacion' : 'Modo Instalacion';
btn.className = active ? 'modern-btn secondary' : 'modern-btn primary';
}
showToast(data.message, active ? 'warning' : 'success');
})
.catch(e => {
console.error("Error:", e);
showToast("Error cambiando modo", "error");
})
.finally(() => { if (btn) btn.disabled = false; });
}
function calibrateBottom() {
if (!confirm("📐 ¿Calibrar fondo del contenedor?\n\nAsegurese de que el contenedor este VACIO.\nSe tomaran 3 lecturas promediadas y se guardara la distancia como altura maxima.")) return;
const btn = document.getElementById('button-calibrate');
if (btn) btn.disabled = true;
showToast("Calibrando fondo... espere", "info");
fetch('/api/calibrate-bottom', { method: 'POST' })
.then(r => r.json())
.then(data => {
if (data.success) {
showToast("Fondo calibrado: " + data.distance + " cm", "success");
} else {
showToast(data.message || "Error calibrando", "error");
}
})
.catch(e => {
console.error("Error:", e);
showToast("Error calibrando fondo", "error");
})
.finally(() => { if (btn) btn.disabled = false; });
}
function setupMultiSensorButton() {
const multiSensorBtn = document.getElementById('multi-sensor-btn');
if (multiSensorBtn) {
// No verificar inmediatamente — loadESPNowSensors() actualiza el estado
setMultiSensorButtonDisabled(); // Empezar deshabilitado
multiSensorBtn.addEventListener('click', openMultiSensorDashboard);
}
}

async function checkESPNowDataAvailability() {
const multiSensorBtn = document.getElementById('multi-sensor-btn');
if (!multiSensorBtn) return;

try {
console.log("🔍 Verificando disponibilidad de datos ESP-NOW...");
const response = await fetch('/multi-sensor-data');

if (response.ok) {
const data = await response.json();

if (data && data.sensors && Array.isArray(data.sensors) && data.sensors.length > 0) {
console.log(`✅ Encontrados ${data.sensors.length} sensores ESP-NOW`);
multiSensorBtn.style.opacity = '1';
multiSensorBtn.style.pointerEvents = 'auto';
multiSensorBtn.title = `Ver ${data.sensors.length} sensores ESP-NOW disponibles`;

// Actualizar texto del botón si hay sensores
const btnText = multiSensorBtn.querySelector('.btn-text') || 
multiSensorBtn.childNodes[multiSensorBtn.childNodes.length - 1];
if (btnText && btnText.textContent) {
btnText.textContent = `Multi-Sensor (${data.sensors.length})`;
}
} else {
console.log("⚠️ No hay sensores ESP-NOW disponibles");
setMultiSensorButtonDisabled();
}
} else {
console.log("⚠️ Endpoint ESP-NOW no disponible");
setMultiSensorButtonDisabled();
}
} catch (error) {
console.log("ℹ️ Datos ESP-NOW no disponibles:", error.message);
setMultiSensorButtonDisabled();
}
}

function setMultiSensorButtonDisabled() {
const multiSensorBtn = document.getElementById('multi-sensor-btn');
if (multiSensorBtn) {
multiSensorBtn.style.opacity = '0.5';
multiSensorBtn.style.pointerEvents = 'none';
multiSensorBtn.title = 'No hay sensores ESP-NOW disponibles';
}
}

function openMultiSensorDashboard() {
console.log("🔗 Abriendo ESP-NOW Dashboard...");
showToast("Abriendo dashboard multi-sensor", "info");
window.open('/espnow_dashboard', '_blank');
}
function showToast(message, type = 'info') {
const toast = document.getElementById('toast');
const toastIcon = document.getElementById('toast-icon');
const toastMessage = document.getElementById('toast-message');
if (!toast || !toastIcon || !toastMessage) return;
const icons = {
success: '✅',
error: '❌',
warning: '⚠️',
info: 'ℹ️'
};
toastIcon.textContent = icons[type] || icons.info;
toastMessage.textContent = message;
toast.classList.add('show');
console.log(`📢 Toast: ${message} (${type})`);
setTimeout(() => {
toast.classList.remove('show');
}, 4000);
}
function formatValue(value) {
if (value === null || value === undefined || value === '') {
return '--';
}
return value.toString();
}
function setupSensorConfig() {
const configButton = document.getElementById('sensor-config-button');
if (configButton) {
configButton.addEventListener('click', (e) => {
e.preventDefault();
showSensorConfigModal();
});
}
}
async function showSensorConfigModal() {
try {
const response = await fetch('/api/sensor/config');
const config = await response.json();
if (config.success) {
createSensorConfigModal(config);
} else {
showToast('Error al cargar configuración', 'error');
}
} catch (error) {
console.error('Error:', error);
showToast('Error de conexión', 'error');
}
}
function createSensorConfigModal(config) {
// Generar opciones del dropdown de modelos de sensor
let sensorModelOptions = '';
if (config.sensor_models) {
config.sensor_models.forEach(m => {
const selected = m.id === config.sensor_model ? 'selected' : '';
const wp = m.waterproof ? ' 💧' : '';
sensorModelOptions += `<option value="${m.id}" ${selected}>${m.name} (${m.min}-${m.max}cm)${wp}</option>`;
});
}
// Generar opciones del dropdown de tipo de contenedor
const containerTypes = [{v:0,l:'Tanque'},{v:1,l:'Cisterna'},{v:2,l:'Contenedor Genérico'}];
let containerOptions = containerTypes.map(t =>
`<option value="${t.v}" ${t.v === config.container_type ? 'selected' : ''}>${t.l}</option>`
).join('');
const isCustom = config.sensor_model === 6;
const modalHTML = `
<div id="sensor-config-modal" class="modal-overlay">
<div class="modal-content">
<div class="modal-header">
<h2>⚙️ Configuración del Sensor</h2>
<button class="modal-close" onclick="closeSensorConfigModal()">&times;</button>
</div>
<div class="modal-body">
<div class="config-section">
<h3>� Modelo de Sensor</h3>
<div class="form-group">
<label>Sensor Ultrasónico</label>
<select id="sensor-model" onchange="onSensorModelChange()">
${sensorModelOptions}
</select>
<small>💧 = Resistente al agua. Selecciona tu modelo para auto-configurar distancias.</small>
</div>
<div class="form-group">
<label>Tipo de Contenedor</label>
<select id="container-type">
${containerOptions}
</select>
</div>
</div>
<div class="config-section">
<h3>📏 Configuración Básica</h3>
<div class="form-group">
<label>Altura del Tanque (cm)</label>
<input type="number" id="tank-height" value="${config.tank_height}" min="50" max="500">
</div>
<div class="form-group">
<label>Capacidad (litros)</label>
<input type="number" id="tank-capacity" value="${config.tank_capacity}" min="100" max="10000">
</div>
<div class="form-group">
<label>Distancia Mínima del Sensor (cm)</label>
<input type="number" id="min-distance" value="${config.min_distance}" min="1" max="100" step="0.1" ${!isCustom ? 'readonly' : ''}>
<small id="min-dist-hint">${!isCustom ? '⚙️ Auto-configurado por modelo de sensor' : 'Ingresa la distancia mínima manualmente'}</small>
</div>
<div class="form-group">
<label>Distancia Máxima del Sensor (cm)</label>
<input type="number" id="max-distance" value="${config.max_distance || 600}" min="50" max="1000" step="0.1" ${!isCustom ? 'readonly' : ''}>
<small id="max-dist-hint">${!isCustom ? '⚙️ Auto-configurado por modelo de sensor' : 'Ingresa la distancia máxima manualmente'}</small>
</div>
</div>
<div class="config-section">
<h3>📡 Configuración de Medición</h3>
<p style="font-size:0.85em;color:#888;margin-bottom:12px;">Cada ciclo toma múltiples lecturas ultrasónicas, filtra el ruido con estadística sigma, y reporta un resultado final a MQTT/Home Assistant.</p>
<h4 style="margin-top:16px;margin-bottom:8px;font-size:0.95em;color:#aaa;">⏱️ Intervalos entre ciclos</h4>
<div class="form-group">
<label>Intervalo normal (segundos)</label>
<input type="number" id="normal-interval" value="${config.normal_interval}" min="15" max="600" step="5">
<small>Cada cuántos segundos se ejecuta un ciclo de sensado. Cada ciclo toma las N lecturas configuradas abajo, aplica el filtro sigma, y publica el resultado. Por defecto: 120s (2 min).</small>
</div>
<div class="form-group" style="background:#1a1a2e;padding:12px;border-radius:8px;border:1px solid #333;">
<label>
<input type="checkbox" id="adaptive-interval" ${config.adaptive_interval ? 'checked' : ''}>
🔄 Detección adaptativa de flujo
</label>
<small style="display:block;margin-top:6px;">Si está activo, detecta llenado/vaciado y temporalmente usa el intervalo rápido. <strong>Desactívalo si quieres intervalos fijos.</strong> Por defecto: OFF.</small>
</div>
<div class="form-group">
<label>Intervalo en llenado/vaciado (segundos)</label>
<input type="number" id="filling-interval" value="${config.filling_interval}" min="5" max="120" step="5">
<small>Intervalo más rápido cuando se detecta que el nivel sube o baja. Solo aplica si "Detección adaptativa" está activa. Por defecto: 30s.</small>
</div>
<div class="form-group">
<label>Umbral de detección de llenado</label>
<input type="number" id="filling-threshold" value="${config.filling_threshold}" min="2" max="5">
<small>Lecturas consecutivas con tendencia sostenida para activar intervalo rápido. Por defecto: 3.</small>
</div>
<h4 style="margin-top:16px;margin-bottom:8px;font-size:0.95em;color:#aaa;">📊 Lecturas por ciclo (filtro sigma)</h4>
<p style="font-size:0.82em;color:#777;margin-bottom:10px;">Dentro de cada ciclo, el sensor toma N lecturas con una pausa entre ellas. Las lecturas con ruido se descartan con el filtro sigma (μ±kσ). El promedio de las supervivientes es el resultado final.</p>
<div class="form-group">
<label>Muestras por ciclo (5-30)</label>
<input type="number" id="sigma-samples" value="${config.sigma_samples || 20}" min="5" max="30" step="1">
<small>Lecturas ultrasónicas por ciclo. Más muestras = más precisión, pero el ciclo tarda más. Tiempo total ≈ N × pausa. Por defecto: 20 (20×300ms = 6s).</small>
</div>
<div class="form-group">
<label>Pausa entre lecturas (ms)</label>
<input type="number" id="sigma-delay-ms" value="${config.sigma_delay_ms || 300}" min="10" max="500" step="10">
<small>Milisegundos entre cada lectura ultrasónica dentro del ciclo. Evita ecos residuales (reverberación) dentro del tanque. Por defecto: 300ms.</small>
</div>
<div class="form-group">
<label>Umbral sigma k (1.0-3.0)</label>
<input type="number" id="sigma-threshold" value="${config.sigma_threshold || 1.5}" min="1.0" max="3.0" step="0.1">
<small>Las lecturas fuera de μ±kσ se descartan. Menor k = más estricto (descarta más ruido). Por defecto: 1.5.</small>
</div>
<div class="form-group">
<label>Mínimo de lecturas válidas (3-20)</label>
<input type="number" id="sigma-min-valid" value="${config.sigma_min_valid || 10}" min="3" max="20" step="1">
<small>Si menos de N lecturas sobreviven el filtro, la medición se descarta como no fiable. Por defecto: 10.</small>
</div>
</div>
<div class="config-section">
<h3>🌍 Configuración Geográfica</h3>
<div class="form-group">
<label>
<input type="checkbox" id="auto-geo" ${config.auto_geo_location ? 'checked' : ''}>
🌐 Detectar ubicación automáticamente
</label>
</div>
<div class="form-group">
<label>🕐 Zona Horaria</label>
<input type="text" id="timezone" value="${config.timezone}" placeholder="America/Mexico_City">
</div>
<div class="form-group">
<label>
<input type="checkbox" id="show-datetime" ${config.show_datetime ? 'checked' : ''}>
📅 Mostrar fecha/hora en pantalla
</label>
</div>
</div>
</div>
<div class="modal-footer">
<button class="modern-btn secondary" onclick="closeSensorConfigModal()">Cancelar</button>
<button class="modern-btn primary" onclick="saveSensorConfig()">💾 Guardar</button>
</div>
</div>
</div>
`;
document.body.insertAdjacentHTML('beforeend', modalHTML);
}
async function saveSensorConfig() {
const config = {
sensor_model: parseInt(document.getElementById('sensor-model').value),
container_type: parseInt(document.getElementById('container-type').value),
tank_height: parseFloat(document.getElementById('tank-height').value),
tank_capacity: parseFloat(document.getElementById('tank-capacity').value),
min_distance: parseFloat(document.getElementById('min-distance').value),
max_distance: parseFloat(document.getElementById('max-distance').value),
normal_interval: parseInt(document.getElementById('normal-interval').value),
adaptive_interval: document.getElementById('adaptive-interval').checked,
filling_interval: parseInt(document.getElementById('filling-interval').value),
filling_threshold: parseInt(document.getElementById('filling-threshold').value),
auto_geo_location: document.getElementById('auto-geo').checked,
timezone: document.getElementById('timezone').value,
show_datetime: document.getElementById('show-datetime').checked,
sigma_samples: parseInt(document.getElementById('sigma-samples').value),
sigma_delay_ms: parseInt(document.getElementById('sigma-delay-ms').value),
sigma_threshold: parseFloat(document.getElementById('sigma-threshold').value),
sigma_min_valid: parseInt(document.getElementById('sigma-min-valid').value)
};
try {
const response = await fetch('/api/sensor/config', {
method: 'POST',
headers: { 'Content-Type': 'application/json' },
body: JSON.stringify(config)
});
const result = await response.json();
if (result.success) {
showToast('⚙️ Configuración guardada exitosamente', 'success');
closeSensorConfigModal();
} else {
showToast('❌ Error: ' + result.message, 'error');
}
} catch (error) {
console.error('Error:', error);
showToast('❌ Error de conexión', 'error');
}
}
function onSensorModelChange() {
const select = document.getElementById('sensor-model');
const modelId = parseInt(select.value);
const isCustom = modelId === 6;
const minInput = document.getElementById('min-distance');
const maxInput = document.getElementById('max-distance');
const minHint = document.getElementById('min-dist-hint');
const maxHint = document.getElementById('max-dist-hint');
if (!isCustom) {
// Buscar specs del modelo seleccionado en el option text
const opt = select.options[select.selectedIndex];
const match = opt.text.match(/\((\d+)-(\d+)cm\)/);
if (match) {
minInput.value = match[1];
maxInput.value = match[2];
}
minInput.readOnly = true;
maxInput.readOnly = true;
if (minHint) minHint.textContent = '⚙️ Auto-configurado por modelo de sensor';
if (maxHint) maxHint.textContent = '⚙️ Auto-configurado por modelo de sensor';
} else {
minInput.readOnly = false;
maxInput.readOnly = false;
if (minHint) minHint.textContent = 'Ingresa la distancia mínima manualmente';
if (maxHint) maxHint.textContent = 'Ingresa la distancia máxima manualmente';
}
}
function closeSensorConfigModal() {
const modal = document.getElementById('sensor-config-modal');
if (modal) {
modal.remove();
}
}
function initContainerSVGs() {
containerSVGs = {
tank: generateTankSVG(),
container: generateContainerSVG(),
cistern: generateCisternSVG()
};
const savedType = localStorage.getItem('containerType') || 'tank';
containerType = savedType;
// NO llamar renderContainerSVG aquí - el HTML ya tiene el SVG del tanque
// Solo renderizar si el tipo guardado es diferente a 'tank'
if (containerType !== 'tank') {
renderContainerSVG();
}
}
function loadAndSetContainerType() {
// NOTA: Reemplazada por loadInitialConfig() que unifica esta función con loadTechSpecs()
loadInitialConfig();
}
function updateMainSensorTitle() {
const titleElement = document.getElementById('main-sensor-title');
if (titleElement) {
const icons = {
tank: '🏠',
container: '🗂️',
cistern: '🏗️'
};
const names = {
tank: 'Tanque Principal',
container: 'Contenedor Principal',
cistern: 'Cisterna Principal'
};
titleElement.textContent = `${icons[containerType]} ${names[containerType]}`;
}
}
function getContainerName(type) {
const names = {
tank: 'Tanque',
container: 'Contenedor',
cistern: 'Cisterna'
};
return names[type] || 'Contenedor';
}
function generateTankSVG() {
return `
<defs>
<linearGradient id="liquidGradient" x1="0%" y1="0%" x2="0%" y2="100%">
<stop offset="0%" style="stop-color:#4FC3F7;stop-opacity:0.9" />
<stop offset="50%" style="stop-color:#29B6F6;stop-opacity:0.8" />
<stop offset="100%" style="stop-color:#0288D1;stop-opacity:1" />
</linearGradient>
<linearGradient id="tankGradient" x1="0%" y1="0%" x2="100%" y2="0%">
<stop offset="0%" style="stop-color:#E0E0E0;stop-opacity:0.9" />
<stop offset="50%" style="stop-color:#F5F5F5;stop-opacity:0.7" />
<stop offset="100%" style="stop-color:#E0E0E0;stop-opacity:0.9" />
</linearGradient>
<pattern id="wavePattern" x="0" y="0" width="100" height="20" patternUnits="userSpaceOnUse">
<path d="M0,10 Q25,0 50,10 T100,10 V20 H0 Z" fill="rgba(255,255,255,0.1)" />
</pattern>
<clipPath id="tankClip">
<path d="M50,80 L250,80 L250,350 A100,15 0 0,0 50,350 Z" />
</clipPath>
</defs>
<!-- Cuerpo del tanque con base eliptica -->
<path d="M50,80 L250,80 L250,350 A100,15 0 0,0 50,350 Z"
fill="url(#tankGradient)" stroke="#B0B0B0" stroke-width="3" opacity="0.8"/>
<!-- Base eliptica inferior (borde visible) -->
<ellipse cx="150" cy="350" rx="100" ry="15" fill="none" stroke="#B0B0B0" stroke-width="2" opacity="0.5"/>
<!-- Tapa del tanque -->
<ellipse cx="150" cy="80" rx="100" ry="15" fill="url(#tankGradient)" stroke="#B0B0B0" stroke-width="3"/>
<ellipse cx="150" cy="75" rx="95" ry="12" fill="#F8F8F8" opacity="0.9"/>
<!-- Liquido animado -->
<g clip-path="url(#tankClip)">
<rect id="liquid-level" x="50" y="370" width="200" height="0"
fill="url(#liquidGradient)" opacity="0.9" class="liquid-animation"/>
<rect id="liquid-waves" x="50" y="370" width="200" height="0"
fill="url(#wavePattern)" opacity="0.6" class="water-waves"/>
</g>
<!-- Medidor lateral izquierdo (porcentaje) -->
<g class="level-indicator">
<rect x="20" y="80" width="20" height="285" fill="rgba(0,0,0,0.1)" rx="10"/>
<rect id="level-bar" x="22" y="363" width="16" height="0" fill="#4CAF50" rx="8"/>
<!-- Marcas del medidor -->
<line x1="15" y1="85" x2="25" y2="85" stroke="#666" stroke-width="2"/>
<text x="10" y="90" fill="#666" font-size="12" text-anchor="end">100%</text>
<line x1="15" y1="155" x2="25" y2="155" stroke="#666" stroke-width="2"/>
<text x="10" y="160" fill="#666" font-size="12" text-anchor="end">75%</text>
<line x1="15" y1="224" x2="25" y2="224" stroke="#666" stroke-width="2"/>
<text x="10" y="229" fill="#666" font-size="12" text-anchor="end">50%</text>
<line x1="15" y1="294" x2="25" y2="294" stroke="#666" stroke-width="2"/>
<text x="10" y="299" fill="#666" font-size="12" text-anchor="end">25%</text>
<line x1="15" y1="363" x2="25" y2="363" stroke="#666" stroke-width="2"/>
<text x="10" y="368" fill="#666" font-size="12" text-anchor="end">0%</text>
</g>
<!-- Medidor lateral derecho (litros) -->
<g id="liters-scale" class="level-indicator">
<rect x="260" y="80" width="20" height="285" fill="rgba(0,0,0,0.1)" rx="10"/>
<!-- Marcas de litros -->
<line x1="275" y1="85" x2="285" y2="85" stroke="#1976D2" stroke-width="2"/>
<text id="liters-100" x="290" y="90" fill="#1976D2" font-size="11" text-anchor="start">--L</text>
<line x1="275" y1="155" x2="285" y2="155" stroke="#1976D2" stroke-width="2"/>
<text id="liters-75" x="290" y="160" fill="#1976D2" font-size="11" text-anchor="start">--L</text>
<line x1="275" y1="224" x2="285" y2="224" stroke="#1976D2" stroke-width="2"/>
<text id="liters-50" x="290" y="229" fill="#1976D2" font-size="11" text-anchor="start">--L</text>
<line x1="275" y1="294" x2="285" y2="294" stroke="#1976D2" stroke-width="2"/>
<text id="liters-25" x="290" y="299" fill="#1976D2" font-size="11" text-anchor="start">--L</text>
<line x1="275" y1="363" x2="285" y2="363" stroke="#1976D2" stroke-width="2"/>
<text id="liters-0" x="290" y="368" fill="#1976D2" font-size="11" text-anchor="start">0L</text>
</g>
<!-- Sensor en la parte superior -->
<circle cx="150" cy="50" r="8" fill="#FF5722" opacity="0.9"/>
<rect x="145" y="42" width="10" height="8" fill="#FF5722" opacity="0.9"/>
<text x="150" y="35" fill="#FF5722" font-size="10" text-anchor="middle" font-weight="bold">SENSOR</text>
`;
}
function generateContainerSVG() {
return `
<defs>
<linearGradient id="liquidGradient" x1="0%" y1="0%" x2="0%" y2="100%">
<stop offset="0%" style="stop-color:#4FC3F7;stop-opacity:0.9" />
<stop offset="50%" style="stop-color:#29B6F6;stop-opacity:0.8" />
<stop offset="100%" style="stop-color:#0288D1;stop-opacity:1" />
</linearGradient>
<pattern id="wavePattern" x="0" y="0" width="80" height="15" patternUnits="userSpaceOnUse">
<path d="M0,8 Q20,0 40,8 T80,8 V15 H0 Z" fill="rgba(255,255,255,0.15)" />
</pattern>
<clipPath id="containerClip">
<rect x="65" y="90" width="170" height="270" rx="3"/>
</clipPath>
</defs>
<!-- Paleta base del IBC -->
<g class="ibc-pallet">
<rect x="50" y="358" width="200" height="8" rx="2" fill="#A1887F" stroke="#795548" stroke-width="1.5"/>
<rect x="55" y="366" width="35" height="22" rx="2" fill="#8D6E63" stroke="#5D4037" stroke-width="1.5"/>
<rect x="133" y="366" width="35" height="22" rx="2" fill="#8D6E63" stroke="#5D4037" stroke-width="1.5"/>
<rect x="210" y="366" width="35" height="22" rx="2" fill="#8D6E63" stroke="#5D4037" stroke-width="1.5"/>
<rect x="50" y="388" width="200" height="6" rx="2" fill="#A1887F" stroke="#795548" stroke-width="1"/>
</g>
<!-- Tanque plástico interior (blanco translúcido) -->
<rect x="65" y="90" width="170" height="270" rx="3"
fill="rgba(240,245,250,0.85)" stroke="#CFD8DC" stroke-width="1"/>
<!-- Líquido animado -->
<g clip-path="url(#containerClip)">
<rect id="liquid-level" x="65" y="365" width="170" height="0"
fill="url(#liquidGradient)" opacity="0.9" class="liquid-animation"/>
<rect id="liquid-waves" x="65" y="365" width="170" height="0"
fill="url(#wavePattern)" opacity="0.6" class="water-waves"/>
</g>
<!-- Jaula metálica del IBC -->
<g class="ibc-cage" stroke="#607D8B" fill="none">
<rect x="57" y="85" width="186" height="275" rx="2" stroke-width="3"/>
<line x1="107" y1="85" x2="107" y2="360" stroke-width="1.5" stroke="#78909C"/>
<line x1="150" y1="85" x2="150" y2="360" stroke-width="1.5" stroke="#78909C"/>
<line x1="193" y1="85" x2="193" y2="360" stroke-width="1.5" stroke="#78909C"/>
<line x1="57" y1="140" x2="243" y2="140" stroke-width="1.5" stroke="#78909C"/>
<line x1="57" y1="195" x2="243" y2="195" stroke-width="1.5" stroke="#78909C"/>
<line x1="57" y1="250" x2="243" y2="250" stroke-width="1.5" stroke="#78909C"/>
<line x1="57" y1="305" x2="243" y2="305" stroke-width="1.5" stroke="#78909C"/>
<line x1="57" y1="85" x2="75" y2="105" stroke-width="1" stroke="#90A4AE"/>
<line x1="243" y1="85" x2="225" y2="105" stroke-width="1" stroke="#90A4AE"/>
</g>
<!-- Tapa/Abertura de llenado -->
<rect x="135" y="76" width="30" height="12" rx="5" fill="#B0BEC5" stroke="#78909C" stroke-width="2"/>
<circle cx="150" cy="82" r="3.5" fill="#90A4AE"/>
<!-- Válvula inferior -->
<rect x="80" y="352" width="14" height="14" rx="3" fill="#78909C" stroke="#546E7A" stroke-width="1.5"/>
<line x1="80" y1="362" x2="68" y2="368" stroke="#546E7A" stroke-width="2.5" stroke-linecap="round"/>
<!-- Medidor lateral izquierdo (porcentaje) -->
<g class="level-indicator">
<rect x="20" y="90" width="18" height="270" fill="rgba(0,0,0,0.1)" rx="9"/>
<rect id="level-bar" x="22" y="360" width="14" height="0" fill="#4CAF50" rx="7"/>
<line x1="15" y1="95" x2="25" y2="95" stroke="#666" stroke-width="2"/>
<text x="10" y="100" fill="#666" font-size="12" text-anchor="end">100%</text>
<line x1="15" y1="158" x2="25" y2="158" stroke="#666" stroke-width="2"/>
<text x="10" y="163" fill="#666" font-size="12" text-anchor="end">75%</text>
<line x1="15" y1="225" x2="25" y2="225" stroke="#666" stroke-width="2"/>
<text x="10" y="230" fill="#666" font-size="12" text-anchor="end">50%</text>
<line x1="15" y1="293" x2="25" y2="293" stroke="#666" stroke-width="2"/>
<text x="10" y="298" fill="#666" font-size="12" text-anchor="end">25%</text>
<line x1="15" y1="360" x2="25" y2="360" stroke="#666" stroke-width="2"/>
<text x="10" y="365" fill="#666" font-size="12" text-anchor="end">0%</text>
</g>
<!-- Medidor lateral derecho (litros) -->
<g id="liters-scale" class="level-indicator">
<rect x="260" y="90" width="18" height="270" fill="rgba(0,0,0,0.1)" rx="9"/>
<line x1="273" y1="95" x2="283" y2="95" stroke="#1976D2" stroke-width="2"/>
<text id="liters-100" x="288" y="100" fill="#1976D2" font-size="11" text-anchor="start">--L</text>
<line x1="273" y1="158" x2="283" y2="158" stroke="#1976D2" stroke-width="2"/>
<text id="liters-75" x="288" y="163" fill="#1976D2" font-size="11" text-anchor="start">--L</text>
<line x1="273" y1="225" x2="283" y2="225" stroke="#1976D2" stroke-width="2"/>
<text id="liters-50" x="288" y="230" fill="#1976D2" font-size="11" text-anchor="start">--L</text>
<line x1="273" y1="293" x2="283" y2="293" stroke="#1976D2" stroke-width="2"/>
<text id="liters-25" x="288" y="298" fill="#1976D2" font-size="11" text-anchor="start">--L</text>
<line x1="273" y1="360" x2="283" y2="360" stroke="#1976D2" stroke-width="2"/>
<text id="liters-0" x="288" y="365" fill="#1976D2" font-size="11" text-anchor="start">0L</text>
</g>
<!-- Sensor en la parte superior -->
<circle cx="150" cy="55" r="8" fill="#FF5722" opacity="0.9"/>
<rect x="145" y="47" width="10" height="8" fill="#FF5722" opacity="0.9"/>
<text x="150" y="40" fill="#FF5722" font-size="10" text-anchor="middle" font-weight="bold">SENSOR</text>
`;
}
function generateCisternSVG() {
return `
<defs>
<linearGradient id="liquidGradient" x1="0%" y1="0%" x2="0%" y2="100%">
<stop offset="0%" style="stop-color:#4FC3F7;stop-opacity:0.9" />
<stop offset="50%" style="stop-color:#29B6F6;stop-opacity:0.8" />
<stop offset="100%" style="stop-color:#0288D1;stop-opacity:1" />
</linearGradient>
<linearGradient id="cisternGradient" x1="0%" y1="0%" x2="100%" y2="0%">
<stop offset="0%" style="stop-color:#795548;stop-opacity:0.9" />
<stop offset="50%" style="stop-color:#8D6E63;stop-opacity:0.7" />
<stop offset="100%" style="stop-color:#795548;stop-opacity:0.9" />
</linearGradient>
<pattern id="wavePattern" x="0" y="0" width="120" height="25" patternUnits="userSpaceOnUse">
<path d="M0,12 Q30,0 60,12 T120,12 V25 H0 Z" fill="rgba(255,255,255,0.1)" />
</pattern>
<clipPath id="cisternClip">
<ellipse cx="150" cy="360" rx="120" ry="20"/>
<rect x="30" y="100" width="240" height="260"/>
</clipPath>
</defs>
<!-- Base de la cisterna (elíptica) -->
<ellipse cx="150" cy="380" rx="125" ry="25" fill="url(#cisternGradient)" stroke="#5D4037" stroke-width="3"/>
<!-- Cuerpo de la cisterna -->
<rect x="25" y="100" width="250" height="280"
fill="url(#cisternGradient)" stroke="#5D4037" stroke-width="3" opacity="0.8"/>
<!-- Tapa de la cisterna -->
<ellipse cx="150" cy="100" rx="125" ry="25" fill="url(#cisternGradient)" stroke="#5D4037" stroke-width="3"/>
<ellipse cx="150" cy="95" rx="120" ry="20" fill="#A1887F" opacity="0.9"/>
<!-- Liquido animado -->
<g clip-path="url(#cisternClip)">
<ellipse id="liquid-level" cx="150" cy="380" rx="120" ry="0"
fill="url(#liquidGradient)" opacity="0.9" class="liquid-animation"/>
<rect id="liquid-body" x="30" y="380" width="240" height="0"
fill="url(#liquidGradient)" opacity="0.9" class="liquid-animation"/>
<rect id="liquid-waves" x="30" y="380" width="240" height="0"
fill="url(#wavePattern)" opacity="0.6" class="water-waves"/>
</g>
<!-- Medidor lateral -->
<g class="level-indicator">
<rect x="10" y="100" width="22" height="280" fill="rgba(0,0,0,0.1)" rx="11"/>
<rect id="level-bar" x="12" y="378" width="18" height="0" fill="#4CAF50" rx="9"/>
<!-- Marcas del medidor -->
<line x1="5" y1="110" x2="15" y2="110" stroke="#666" stroke-width="2"/>
<text x="0" y="115" fill="#666" font-size="12" text-anchor="end">100%</text>
<line x1="5" y1="170" x2="15" y2="170" stroke="#666" stroke-width="2"/>
<text x="0" y="175" fill="#666" font-size="12" text-anchor="end">75%</text>
<line x1="5" y1="240" x2="15" y2="240" stroke="#666" stroke-width="2"/>
<text x="0" y="245" fill="#666" font-size="12" text-anchor="end">50%</text>
<line x1="5" y1="310" x2="15" y2="310" stroke="#666" stroke-width="2"/>
<text x="0" y="315" fill="#666" font-size="12" text-anchor="end">25%</text>
<line x1="5" y1="375" x2="15" y2="375" stroke="#666" stroke-width="2"/>
<text x="0" y="380" fill="#666" font-size="12" text-anchor="end">0%</text>
</g>
<!-- Escalera de acceso -->
<g stroke="#5D4037" stroke-width="2" fill="none">
<line x1="280" y1="100" x2="280" y2="380"/>
<line x1="275" y1="130" x2="285" y2="130"/>
<line x1="275" y1="160" x2="285" y2="160"/>
<line x1="275" y1="190" x2="285" y2="190"/>
<line x1="275" y1="220" x2="285" y2="220"/>
<line x1="275" y1="250" x2="285" y2="250"/>
<line x1="275" y1="280" x2="285" y2="280"/>
<line x1="275" y1="310" x2="285" y2="310"/>
<line x1="275" y1="340" x2="285" y2="340"/>
<line x1="275" y1="370" x2="285" y2="370"/>
</g>
<!-- Medidor lateral derecho (litros) -->
<g id="liters-scale" class="level-indicator">
<rect x="295" y="100" width="16" height="280" fill="rgba(0,0,0,0.1)" rx="8"/>
<line x1="306" y1="110" x2="315" y2="110" stroke="#1976D2" stroke-width="2"/>
<text id="liters-100" x="319" y="115" fill="#1976D2" font-size="10" text-anchor="start">--L</text>
<line x1="306" y1="170" x2="315" y2="170" stroke="#1976D2" stroke-width="2"/>
<text id="liters-75" x="319" y="175" fill="#1976D2" font-size="10" text-anchor="start">--L</text>
<line x1="306" y1="240" x2="315" y2="240" stroke="#1976D2" stroke-width="2"/>
<text id="liters-50" x="319" y="245" fill="#1976D2" font-size="10" text-anchor="start">--L</text>
<line x1="306" y1="310" x2="315" y2="310" stroke="#1976D2" stroke-width="2"/>
<text id="liters-25" x="319" y="315" fill="#1976D2" font-size="10" text-anchor="start">--L</text>
<line x1="306" y1="375" x2="315" y2="375" stroke="#1976D2" stroke-width="2"/>
<text id="liters-0" x="319" y="380" fill="#1976D2" font-size="10" text-anchor="start">0L</text>
</g>
<!-- Sensor en la parte superior -->
<circle cx="150" cy="70" r="8" fill="#FF5722" opacity="0.9"/>
<rect x="145" y="62" width="10" height="8" fill="#FF5722" opacity="0.9"/>
<text x="150" y="55" fill="#FF5722" font-size="10" text-anchor="middle" font-weight="bold">SENSOR</text>
`;
}
function renderContainerSVG() {
const svgElement = document.getElementById('liquid-container-svg');
if (!svgElement || !containerSVGs[containerType]) return;
svgElement.innerHTML = containerSVGs[containerType];
console.log('SVG renderizado, tipo:', containerType);
// Re-aplicar datos actuales después de un pequeño delay
// para asegurar que el DOM se actualice
if (currentData && currentData.Porcentaje !== undefined) {
setTimeout(() => {
try { updateContainerAnimation(currentData); } catch (e) { console.error('Error re-animando:', e); }
}, 200);
}
}
function updateContainerAnimation(data) {
const percentage = parseFloat(data.Porcentaje) || 0;
const isValid = percentage >= 0 && percentage <= 100;
if (!isValid) {
console.warn('Datos de sensor inválidos:', data);
return;
}
switch (containerType) {
case 'tank':
updateTankAnimation(data);
break;
case 'container':
updateContainerAnimationRect(data);
break;
case 'cistern':
updateCisternAnimation(data);
break;
}
}
function getSVGElement(id) {
// Buscar primero con getElementById, luego con querySelector como fallback
let el = document.getElementById(id);
if (!el) {
const svg = document.getElementById('liquid-container-svg');
if (svg) el = svg.querySelector('#' + id);
}
if (!el) {
// Último recurso: buscar por atributo
const svg = document.getElementById('liquid-container-svg');
if (svg) {
const all = svg.querySelectorAll('[id]');
for (let i = 0; i < all.length; i++) {
if (all[i].getAttribute('id') === id) { el = all[i]; break; }
}
}
}
return el;
}
function updateTankAnimation(data) {
const percentage = parseFloat(data.Porcentaje) || 0;
const liquidLevel = getSVGElement('liquid-level');
const liquidWaves = getSVGElement('liquid-waves');
const levelBar = getSVGElement('level-bar');
console.log('🎨 Tank anim:', percentage + '%', 'elements:', !!liquidLevel, !!liquidWaves, !!levelBar);
if (!liquidLevel || !liquidWaves || !levelBar) {
console.error('❌ SVG elements not found!');
return;
}
const maxHeight = 290;
const height = (percentage / 100) * maxHeight;
const yPosition = 370 - height;
liquidLevel.setAttribute('y', String(yPosition));
liquidLevel.setAttribute('height', String(height));
liquidWaves.setAttribute('y', String(yPosition));
liquidWaves.setAttribute('height', String(Math.min(height, 20)));
levelBar.setAttribute('y', String(363 - (percentage / 100) * 283));
levelBar.setAttribute('height', String((percentage / 100) * 283));
const color = percentage > 50 ? '#4CAF50' :
percentage > 25 ? '#FF9800' : '#F44336';
levelBar.setAttribute('fill', color);
updateLiquidColor(percentage);
console.log('✅ Tank animado: y=' + yPosition + ' h=' + height);
}
function updateLitersScale(capacity) {
const scales = [
{ id: 'liters-100', pct: 100 },
{ id: 'liters-75', pct: 75 },
{ id: 'liters-50', pct: 50 },
{ id: 'liters-25', pct: 25 },
{ id: 'liters-0', pct: 0 }
];
scales.forEach(function(s) {
const el = getSVGElement(s.id);
if (el) {
const liters = Math.round(capacity * s.pct / 100);
el.textContent = liters + 'L';
}
});
console.log('📏 Escala de litros actualizada: 0-' + capacity + 'L');
}
function updateContainerAnimationRect(data) {
const percentage = parseFloat(data.Porcentaje) || 0;
const liquidLevel = getSVGElement('liquid-level');
const liquidWaves = getSVGElement('liquid-waves');
const levelBar = getSVGElement('level-bar');
if (liquidLevel && liquidWaves && levelBar) {
const maxHeight = 275; // Altura del tanque IBC (body y=90 a y=360 + margen)
const height = (percentage / 100) * maxHeight;
const yPosition = 365 - height;
liquidLevel.setAttribute('y', yPosition);
liquidLevel.setAttribute('height', height);
liquidWaves.setAttribute('y', yPosition);
liquidWaves.setAttribute('height', Math.min(height, 15));
const barMaxHeight = 270;
const barHeight = (percentage / 100) * barMaxHeight;
levelBar.setAttribute('y', 360 - barHeight);
levelBar.setAttribute('height', barHeight);
const color = percentage > 50 ? '#4CAF50' :
percentage > 25 ? '#FF9800' : '#F44336';
levelBar.setAttribute('fill', color);
updateLiquidColor(percentage);
}
}
function updateCisternAnimation(data) {
const percentage = parseFloat(data.Porcentaje) || 0;
const liquidLevel = getSVGElement('liquid-level');
const waterBody = getSVGElement('liquid-body');
const liquidWaves = getSVGElement('liquid-waves');
const levelBar = getSVGElement('level-bar');
if (liquidLevel && waterBody && liquidWaves && levelBar) {
const maxHeight = 280; // Altura de la cisterna
const height = (percentage / 100) * maxHeight;
const yPosition = 380 - height;
const ellipseHeight = Math.min(height, 25);
liquidLevel.setAttribute('ry', ellipseHeight);
liquidLevel.setAttribute('cy', 380 - height + ellipseHeight);
if (height > 25) {
waterBody.setAttribute('y', yPosition);
waterBody.setAttribute('height', height - 25);
} else {
waterBody.setAttribute('height', 0);
}
liquidWaves.setAttribute('y', yPosition);
liquidWaves.setAttribute('height', Math.min(height, 25));
levelBar.setAttribute('y', 378 - height);
levelBar.setAttribute('height', height);
const color = percentage > 50 ? '#4CAF50' :
percentage > 25 ? '#FF9800' : '#F44336';
levelBar.setAttribute('fill', color);
}
}
function loadESPNowSensors() {
console.log("🔍 Cargando sensores ESP-NOW...");
fetch('/multi-sensor-data')
.then(response => response.json())
.then(data => {
console.log("📡 Datos ESP-NOW recibidos:", data);
if (data.sensors && Array.isArray(data.sensors)) {
espNowSensors = data.sensors;
renderESPNowSensors();
networkStatus.connected = true;
networkStatus.deviceCount = data.sensors.length;
networkStatus.lastUpdate = new Date();
} else {
espNowSensors = [];
networkStatus.connected = false;
networkStatus.deviceCount = 0;
}
updateNetworkStatus();
})
.catch(error => {
console.error("❌ Error cargando sensores ESP-NOW:", error);
espNowSensors = [];
networkStatus.connected = false;
networkStatus.deviceCount = 0;
updateNetworkStatus();
});
}
function renderESPNowSensors() {
const container = document.getElementById('esp-now-sensors');
if (!container) return;
container.innerHTML = '';
if (espNowSensors.length === 0) {
container.innerHTML = `
<div class="esp-now-sensor offline">
<div class="esp-sensor-header">
<div class="esp-sensor-title">
📡 Sin sensores ESP-NOW
</div>
</div>
<p style="color: var(--text-secondary); margin-top: 10px;">
No se encontraron sensores adicionales en la red mesh.
</p>
</div>
`;
return;
}
espNowSensors.forEach((sensor, index) => {
const sensorElement = createESPNowSensorElement(sensor, index);
container.appendChild(sensorElement);
});
}
function createESPNowSensorElement(sensor, index) {
const sensorDiv = document.createElement('div');
sensorDiv.className = `esp-now-sensor ${sensor.online ? '' : 'offline'}`;
sensorDiv.id = `esp-sensor-${sensor.id || index}`;
const signalStrength = sensor.rssi || -70;
const signalBars = Math.max(1, Math.min(4, Math.floor((signalStrength + 100) / 15)));
sensorDiv.innerHTML = `
<div class="esp-sensor-header">
<div class="esp-sensor-title">
📡 ${sensor.name || `Sensor ${index + 1}`}
<span class="esp-sensor-id">ID: ${sensor.id || index + 1}</span>
</div>
<div class="esp-signal-strength">
<span>${signalStrength}dBm</span>
<div class="signal-bars">
${Array.from({length: 4}, (_, i) =>
`<div class="signal-bar bar-${i+1} ${i < signalBars ? 'active' : ''}"></div>`
).join('')}
</div>
</div>
</div>
<div class="esp-sensor-visual">
<svg viewBox="0 0 200 150" class="esp-container-svg">
<!-- Mini contenedor -->
<rect x="40" y="30" width="120" height="90" rx="5"
fill="rgba(255,255,255,0.1)" stroke="#666" stroke-width="2"/>
<!-- Liquido -->
<rect x="42" y="${120 - (sensor.percentage || 0) * 0.88}"
width="116" height="${(sensor.percentage || 0) * 0.88}"
fill="url(#liquidGradient)" opacity="0.8"/>
<!-- Sensor -->
<circle cx="100" cy="20" r="4" fill="#FF5722"/>
<text x="100" y="15" fill="#FF5722" font-size="8" text-anchor="middle">S</text>
</svg>
</div>
<div class="esp-sensor-data">
<div class="data-item">
<span style="font-size: 0.9rem;">💧</span>
<div class="data-content">
<span class="data-value">${sensor.liquidLevel || '--'}</span>
<span class="data-unit">L</span>
</div>
</div>
<div class="data-item">
<span style="font-size: 0.9rem;">%</span>
<div class="data-content">
<span class="data-value">${sensor.percentage || '--'}</span>
<span class="data-unit">%</span>
</div>
</div>
<div class="data-item">
<span style="font-size: 0.9rem;">📏</span>
<div class="data-content">
<span class="data-value">${sensor.distance || '--'}</span>
<span class="data-unit">cm</span>
</div>
</div>
</div>
${sensor.lastUpdate ? `
<div class="esp-sensor-timestamp">
<span style="font-size: 0.8rem; color: var(--text-secondary);">
Actualizado: ${new Date(sensor.lastUpdate).toLocaleTimeString()}
</span>
</div>
` : ''}
`;
return sensorDiv;
}
function updateNetworkStatus() {
let statusElement = document.querySelector('.network-status');
if (!statusElement) {
statusElement = document.createElement('div');
statusElement.className = 'network-status';
const sensorsGrid = document.querySelector('.sensors-grid');
if (sensorsGrid && sensorsGrid.parentNode) {
sensorsGrid.parentNode.insertBefore(statusElement, sensorsGrid);
}
}
const isOnline = networkStatus.connected && networkStatus.deviceCount > 0;
statusElement.className = `network-status ${isOnline ? '' : 'offline'}`;
statusElement.innerHTML = `
<div class="network-status-icon">
${isOnline ? '🌐' : '📴'}
</div>
<div class="network-status-text">
<div class="network-status-title">
Red ESP-NOW ${isOnline ? 'Activa' : 'Inactiva'}
</div>
<div class="network-status-detail">
${isOnline ?
`${networkStatus.deviceCount} sensores conectados` :
'Sin sensores adicionales detectados'
}
${networkStatus.lastUpdate ?
` • Última actualización: ${networkStatus.lastUpdate.toLocaleTimeString()}` :
''
}
</div>
</div>
`;
}
function startESPNowPolling() {
setInterval(() => {
loadESPNowSensors();
}, 15000);
console.log("🔄 Polling ESP-NOW iniciado (cada 15 segundos)");
}
function logSensorData() {
console.table({
main: currentData,
espNow: espNowSensors,
network: networkStatus
});
}

// ===== LOG DE MEDICIONES EN DASHBOARD =====
let logPaused = false;
let lastPumpState = null;
const MAX_LOG_ENTRIES = 200;

function initSensorLog() {
    const pauseBtn = document.getElementById('log-pause-btn');
    const clearBtn = document.getElementById('log-clear-btn');
    if (pauseBtn) {
        pauseBtn.addEventListener('click', () => {
            logPaused = !logPaused;
            const icon = pauseBtn.querySelector('.material-symbols-outlined');
            if (icon) icon.textContent = logPaused ? 'play_arrow' : 'pause';
            pauseBtn.title = logPaused ? 'Reanudar' : 'Pausar';
        });
    }
    if (clearBtn) {
        clearBtn.addEventListener('click', () => {
            const logEl = document.getElementById('sensor-log');
            if (logEl) logEl.innerHTML = '<div class=\"log-placeholder\">Log limpiado</div>';
        });
    }
    // Historial se carga con delay desde el init principal
    // loadSensorHistory() se llama desde setTimeout en el init
}

async function loadSensorHistory() {
    try {
        const resp = await fetch('/api/sensor-history');
        if (!resp.ok) return;
        const data = await resp.json();
        if (data.entries && data.entries.length > 0) {
            const logEl = document.getElementById('sensor-log');
            if (!logEl) return;
            logEl.innerHTML = '';
            data.entries.forEach(e => {
                const dt = e.t > 0 ? formatUnixTime(e.t) : '--:--:--';
                appendLogEntry(e.l, e.d, e.p, e.s, dt, false);
            });
            scrollLogToBottom();
        }
    } catch (err) {
        console.error('Error cargando historial:', err);
    }
}

function formatUnixTime(ts) {
    const d = new Date(ts * 1000);
    const pad = n => String(n).padStart(2, '0');
    return pad(d.getHours()) + ':' + pad(d.getMinutes()) + ':' + pad(d.getSeconds()) + '.000';
}

function addSensorLogEntry(data) {
    if (logPaused) return;
    // Solo registrar cuando hay una lectura nueva del sensor (readingId cambia)
    // Y respetar el intervalo mínimo entre registros del log
    if (data.readingId !== undefined) {
        if (addSensorLogEntry._lastId === data.readingId) return;
        addSensorLogEntry._lastId = data.readingId;
    }
    // Throttle: no registrar más seguido que el intervalo normal configurado
    var now = Date.now();
    var minInterval = (addSensorLogEntry._minInterval || 30) * 1000;
    if (addSensorLogEntry._lastTime && (now - addSensorLogEntry._lastTime) < minInterval - 2000) return;
    addSensorLogEntry._lastTime = now;
    const dt = data.time || new Date().toLocaleTimeString('es-MX', {hour:'2-digit',minute:'2-digit',second:'2-digit',fractionalSecondDigits:3});
    const dist = data.distancia !== undefined ? data.distancia : '--';
    const modo = data.installationMode ? 'INSTALACION' : (data.estadoSensor === 'Calibrando' ? 'CALIBRANDO' : 'Normal');
    if (data.installationMode) {
        // En modo instalación solo mostrar distancia (igual que WebSerial y OLED)
        appendLogEntry(null, dist, null, null, dt, true, modo);
    } else {
        const lit = data.litros !== undefined ? data.litros : '--';
        const pct = data.porcentaje !== undefined ? data.porcentaje : '--';
        const estado = data.estadoSensor || '--';
        // Obtener lecturas RAW del ciclo y mostrar antes del resultado
        fetch('/api/last-readings').then(r => r.json()).then(details => {
            if (details.raw && details.raw.length > 0) {
                appendRawReadingsEntry(details, dt);
            }
        }).catch(() => {});
        appendLogEntry(lit, dist, pct, estado, dt, true, modo);
    }
}

function appendRawReadingsEntry(d, dt) {
    const logEl = document.getElementById('sensor-log');
    if (!logEl) return;
    const ph = logEl.querySelector('.log-placeholder');
    if (ph) ph.remove();
    
    const entry = document.createElement('div');
    entry.className = 'log-entry';
    entry.style.cssText = 'opacity:0.7;font-size:0.85em;';
    
    const timeSpan = document.createElement('span');
    timeSpan.className = 'log-time';
    timeSpan.textContent = dt;
    
    const dataSpan = document.createElement('span');
    dataSpan.className = 'log-data';
    
    // Construir resumen compacto de lecturas
    let parts = [];
    const flagMap = {O:'✓', S:'⚡', X:'✘', E:'❌'};
    for (let i = 0; i < d.n; i++) {
        const v = d.raw[i] < 0 ? 'ERR' : d.raw[i].toFixed(1);
        const f = flagMap[d.f[i]] || '?';
        const isRej = d.f[i] !== 'O';
        parts.push(isRej ? '(' + v + f + ')' : v + f);
    }
    
    const ok = (d.f.match(/O/g) || []).length;
    const spk = (d.f.match(/S/g) || []).length;
    const sig = (d.f.match(/X/g) || []).length;
    const err = (d.f.match(/E/g) || []).length;
    
    let summary = '🔬 ' + d.n + ' lecturas → ' + d.surv + ' válidas';
    if (spk > 0) summary += ' | ' + spk + ' spike';
    if (sig > 0) summary += ' | ' + sig + ' σ-rej';
    if (err > 0) summary += ' | ' + err + ' error';
    summary += ' | μ:' + d.mean.toFixed(1) + 'cm σ:' + d.std.toFixed(2);
    summary += '\n  ' + parts.join(' ');
    
    dataSpan.style.whiteSpace = 'pre-wrap';
    dataSpan.textContent = summary;
    
    entry.appendChild(timeSpan);
    entry.appendChild(dataSpan);
    logEl.appendChild(entry);
}

function appendLogEntry(lit, dist, pct, estado, dt, doScroll, modo) {
    const logEl = document.getElementById('sensor-log');
    if (!logEl) return;
    // Limpiar placeholder
    const ph = logEl.querySelector('.log-placeholder');
    if (ph) ph.remove();
    
    const entry = document.createElement('div');
    entry.className = 'log-entry';
    
    const timeSpan = document.createElement('span');
    timeSpan.className = 'log-time';
    timeSpan.textContent = dt;
    
    const dataSpan = document.createElement('span');
    dataSpan.className = 'log-data';
    let logText;
    if (lit === null && pct === null) {
        // Modo instalación: solo distancia
        logText = '\u{1F4CF} Distancia: ' + dist + ' cm';
    } else {
        logText = lit + 'L | ' + dist + 'cm | ' + pct + '% | ' + estado + ' \u25C0 RESULTADO';
    }
    if (modo && modo !== 'Normal') logText += ' [' + modo + ']';
    dataSpan.textContent = logText;
    
    entry.appendChild(timeSpan);
    entry.appendChild(dataSpan);
    logEl.appendChild(entry);
    
    // Limitar entradas
    while (logEl.children.length > MAX_LOG_ENTRIES) {
        logEl.removeChild(logEl.firstChild);
    }
    
    if (doScroll) scrollLogToBottom();
}

function scrollLogToBottom() {
    const logEl = document.getElementById('sensor-log');
    if (logEl) logEl.scrollTop = logEl.scrollHeight;
}

function checkPumpStateChange(data) {
    if (data.pumpActive === undefined) return;
    var newState = !!data.pumpActive;
    if (lastPumpState !== null && newState !== lastPumpState) {
        var dt = data.time || new Date().toLocaleTimeString('es-MX', {hour:'2-digit',minute:'2-digit',second:'2-digit',fractionalSecondDigits:3});
        appendPumpLogEntry(newState, dt);
    }
    lastPumpState = newState;
}

function appendPumpLogEntry(isOn, dt) {
    var logEl = document.getElementById('sensor-log');
    if (!logEl) return;
    var ph = logEl.querySelector('.log-placeholder');
    if (ph) ph.remove();
    var entry = document.createElement('div');
    entry.className = 'log-entry';
    entry.style.cssText = 'background:' + (isOn ? 'rgba(33,150,243,0.15)' : 'rgba(255,152,0,0.15)') + ';border-left:3px solid ' + (isOn ? '#2196F3' : '#FF9800') + ';';
    var timeSpan = document.createElement('span');
    timeSpan.className = 'log-time';
    timeSpan.textContent = dt;
    var dataSpan = document.createElement('span');
    dataSpan.className = 'log-data';
    dataSpan.style.fontWeight = 'bold';
    dataSpan.textContent = isOn ? '\uD83D\uDCA7 BOMBA ENCENDIDA v\u00EDa MQTT \u2014 intervalo r\u00E1pido activado' : '\uD83D\uDED1 BOMBA APAGADA v\u00EDa MQTT \u2014 intervalo normal';
    entry.appendChild(timeSpan);
    entry.appendChild(dataSpan);
    logEl.appendChild(entry);
    while (logEl.children.length > MAX_LOG_ENTRIES) {
        logEl.removeChild(logEl.firstChild);
    }
    scrollLogToBottom();
}

// ===== HOME ASSISTANT MQTT CONFIGURATION =====
function setupMQTTConfig() {
    const btn = document.getElementById('mqtt-config-button');
    if (btn) {
        btn.addEventListener('click', (e) => {
            e.preventDefault();
            showMQTTConfigModal();
        });
    }
}

async function showMQTTConfigModal() {
    try {
        const response = await fetch('/api/mqtt/config');
        const config = await response.json();
        if (config.success) {
            createMQTTConfigModal(config);
        } else {
            showToast('Error al cargar configuración MQTT', 'error');
        }
    } catch (error) {
        console.error('Error:', error);
        showToast('Error de conexión', 'error');
    }
}

function createMQTTConfigModal(config) {
    const statusColor = config.connected ? '#4CAF50' : (config.enabled ? '#FF9800' : '#666');
    const statusText = config.connected ? '✅ Conectado' : (config.enabled ? '⏳ ' + (config.status || 'Desconectado') : '⏸️ Deshabilitado');

    const modalHTML = `
<div id="mqtt-config-modal" class="modal-overlay">
<div class="modal-content">
<div class="modal-header">
<h2>🏠 Configuración MQTT - Home Assistant</h2>
<button class="modal-close" onclick="closeMQTTConfigModal()">&times;</button>
</div>
<div class="modal-body">
<div class="config-section">
<h3>📡 Estado de Conexión</h3>
<div class="form-group">
<div style="display:flex;align-items:center;gap:8px;padding:10px;background:rgba(255,255,255,0.05);border-radius:8px;">
<span style="width:12px;height:12px;border-radius:50%;background:${statusColor};display:inline-block;"></span>
<span>${statusText}</span>
</div>
</div>
</div>
<div class="config-section">
<h3>🔌 Broker MQTT (Mosquitto)</h3>
<div class="form-group">
<label>Dirección IP del Broker</label>
<input type="text" id="mqtt-broker-host" value="${config.brokerHost || ''}" placeholder="Ej: 192.168.1.100">
<small>IP o hostname de tu servidor Mosquitto / Home Assistant</small>
</div>
<div class="form-group">
<label>Puerto</label>
<input type="number" id="mqtt-broker-port" value="${config.brokerPort || 1883}" min="1" max="65535">
<small>Puerto del broker MQTT (default: 1883)</small>
</div>
</div>
<div class="config-section">
<h3>🔑 Autenticación (opcional)</h3>
<div class="form-group">
<label>Usuario MQTT</label>
<input type="text" id="mqtt-username" value="${config.username || ''}" placeholder="Ej: homeassistant">
<small>Dejar vacío si no requiere autenticación</small>
</div>
<div class="form-group">
<label>Contraseña MQTT</label>
<input type="password" id="mqtt-password" value="${config.password || ''}" placeholder="Contraseña del broker">
</div>
</div>
<div class="config-section">
<h3>📋 Dispositivo</h3>
<div class="form-group">
<label>Nombre del Dispositivo</label>
<input type="text" id="mqtt-device-name" value="${config.deviceName || 'Sensor Nivel Agua'}" placeholder="Nombre visible en HA">
<small>Nombre que aparecerá en Home Assistant</small>
</div>
<div class="form-group">
<label>
<input type="checkbox" id="mqtt-enabled" ${config.enabled ? 'checked' : ''}>
Habilitar conexión MQTT
</label>
<small>Desactiva para ahorrar recursos si no usas Home Assistant</small>
</div>
</div>
<div class="config-section" style="background:rgba(33,150,243,0.08);border-radius:8px;padding:12px;">
<h3>ℹ️ Auto-Discovery</h3>
<p style="font-size:0.85em;opacity:0.8;margin:0;">Al conectarse, el sensor se registra automáticamente en Home Assistant vía MQTT Discovery. Aparecerá como un dispositivo con entidades de litros, porcentaje, distancia, alertas y señal WiFi.</p>
</div>
</div>
<div class="modal-footer">
<button class="modern-btn secondary" onclick="closeMQTTConfigModal()">Cancelar</button>
<button class="modern-btn primary" onclick="saveMQTTConfig()">💾 Guardar</button>
</div>
</div>
</div>
`;
    document.body.insertAdjacentHTML('beforeend', modalHTML);
}

async function saveMQTTConfig() {
    const config = {
        brokerHost: document.getElementById('mqtt-broker-host').value.trim(),
        brokerPort: parseInt(document.getElementById('mqtt-broker-port').value) || 1883,
        username: document.getElementById('mqtt-username').value.trim(),
        password: document.getElementById('mqtt-password').value.trim(),
        deviceName: document.getElementById('mqtt-device-name').value.trim() || 'Sensor Nivel Agua',
        enabled: document.getElementById('mqtt-enabled').checked
    };

    if (config.enabled && !config.brokerHost) {
        showToast('⚠️ Ingresa la dirección IP del broker MQTT', 'warning');
        return;
    }

    try {
        const response = await fetch('/api/mqtt/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(config)
        });
        const result = await response.json();
        if (result.success) {
            showToast('🏠 Configuración MQTT guardada', 'success');
            closeMQTTConfigModal();
        } else {
            showToast('❌ Error: ' + result.message, 'error');
        }
    } catch (error) {
        console.error('Error:', error);
        showToast('❌ Error de conexión', 'error');
    }
}

function closeMQTTConfigModal() {
    const modal = document.getElementById('mqtt-config-modal');
    if (modal) modal.remove();
}

window.smartSensor = {
data: currentData,
espNowSensors,
networkStatus,
containerType,
isConnected,
logData: logSensorData,
showToast,
updateContainerAnimation: () => updateContainerAnimation(currentData),
showSensorConfig: showSensorConfigModal,
showMQTTConfig: showMQTTConfigModal,
loadESPNow: loadESPNowSensors,
switchContainer: (type) => {
document.getElementById('container-type').value = type;
document.getElementById('container-type').dispatchEvent(new Event('change'));
}
};
console.log("💧 Smart Liquid Sensor Dashboard Unificado cargado completamente!");
