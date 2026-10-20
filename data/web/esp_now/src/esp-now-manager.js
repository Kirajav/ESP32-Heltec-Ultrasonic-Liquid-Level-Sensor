/**
 * ========================================
 * ESP-NOW NETWORK MANAGER JAVASCRIPT
 * ========================================
 */

// --- INITIALIZATION ---
document.addEventListener('DOMContentLoaded', function() {
    console.log('🚀 ESP-NOW Manager iniciado');
    initializeESPNowManager();
    loadNetworkData();
    // Do not start polling with mock data, it will just re-render the same thing
    // startDataPolling(); 
});

function initializeESPNowManager() {
    setupEventListeners();
    loadSavedTheme();
    initializeLanguage();
    loadNetworkData(); // Cargar datos reales en lugar de mock
}function setupEventListeners() {
    const themeToggleButton = document.getElementById('theme-toggle');
    if (themeToggleButton) {
        themeToggleButton.addEventListener('click', toggleTheme);
        console.log('✅ Theme button listener agregado');
    }
    
    const languageToggleButton = document.getElementById('language-toggle');
    if (languageToggleButton) {
        languageToggleButton.addEventListener('click', (event) => {
            event.stopPropagation();
            event.preventDefault();
            toggleLanguage(event);
        });
    }
    
    const topologyContainer = document.getElementById('network-topology');
    if(topologyContainer){
        new ResizeObserver(renderNetworkTopology).observe(topologyContainer);
    }
    console.log('Event listeners configured');
}

// --- THEME SUPPORT ---
function toggleTheme() {
    document.body.classList.toggle('light-theme');
    const isLight = document.body.classList.contains('light-theme');
    localStorage.setItem('global-theme', isLight ? 'light' : 'dark');
    const themeIcon = document.querySelector('#theme-toggle .material-symbols-outlined');
    if (themeIcon) {
        themeIcon.textContent = isLight ? 'dark_mode' : 'light_mode';
    }
}

function loadSavedTheme() {
    const savedTheme = localStorage.getItem('global-theme') || 'dark';
    const themeIcon = document.querySelector('#theme-toggle .material-symbols-outlined');
    if (savedTheme === 'light') {
        document.body.classList.add('light-theme');
        if (themeIcon) themeIcon.textContent = 'dark_mode';
    } else {
        document.body.classList.remove('light-theme');
        if (themeIcon) themeIcon.textContent = 'light_mode';
    }
}

// --- NETWORK TOPOLOGY RENDER (CORRECTED) ---
function renderNetworkTopology() {
    const container = document.getElementById('network-topology');
    if (!container) return;

    container.innerHTML = ''; // Clear previous render

    const containerRect = container.getBoundingClientRect();
    // Handle case where container might not have dimensions yet
    if (containerRect.width === 0 || containerRect.height === 0) {
        return; 
    }

    // Ajustar posiciones para mejor distribución
    const centerX = containerRect.width / 2;
    const centerY = containerRect.height / 2;
    const masterX = centerX - 100; // Mover master más a la izquierda
    const masterY = centerY;

    // --- Render Master Node (Este dispositivo como hub) ---
    const masterNode = document.createElement('div');
    masterNode.className = 'node master-node';
    masterNode.style.left = `${masterX}px`;
    masterNode.style.top = `${masterY}px`;
    masterNode.innerHTML = `
        <div class="node-circle">
            <span class="node-icon">🎯</span>
        </div>
        <div class="node-label">
            <div class="node-name">${getTranslation('masterNode')}</div>
            <div class="node-details">${getTranslation('centralHub')}</div>
        </div>
    `;
    container.appendChild(masterNode);

    const slaves = networkData.connectedDevices || [];
    if (slaves.length === 0) {
        const emptyState = document.createElement('div');
        emptyState.className = 'empty-state';
        emptyState.innerHTML = `<div class="empty-state-icon material-symbols-outlined">hub</div><p class="empty-state-message">${getTranslation('noSlaveDevices')}</p>`;
        container.appendChild(emptyState);
        return;
    }

    const radius = Math.min(containerRect.width, containerRect.height) / 2.8; // Adjusted radius for better spacing
    const angleStep = (2 * Math.PI) / slaves.length;

    // --- Render Slave Nodes ---
    slaves.forEach((device, index) => {
        const angle = angleStep * index - (Math.PI / 2);
        const slaveX = masterX + radius * Math.cos(angle);
        const slaveY = masterY + radius * Math.sin(angle);

        const slaveNode = document.createElement('div');
        slaveNode.className = 'node slave-node';
        slaveNode.style.left = `${slaveX}px`;
        slaveNode.style.top = `${slaveY}px`;
        slaveNode.innerHTML = `
            <div class="node-circle">
                <span class="node-icon">📡</span>
            </div>
            <div class="node-label">
                <div class="node-name">${device.name || `Sensor ${device.id}`}</div>
                <div class="node-details">
                    Señal: ${device.rssi || device.signal || '-?'} dBm<br>
                    Nivel: ${device.liquidLevel || '--'}%<br>
                    Litros: ${device.liters || '--'}L
                </div>
            </div>
        `;
        container.appendChild(slaveNode);

        // --- Render Connection Line (Mejorada) ---
        const line = document.createElement('div');
        line.className = 'connection-line';
        
        const nodeRadius = 35; // Radio del círculo del nodo (70px/2)
        const distance = Math.hypot(slaveX - masterX, slaveY - masterY);
        const lineLength = distance - (nodeRadius * 2); // Acortar para que no se solape con círculos
        const rotation = Math.atan2(slaveY - masterY, slaveX - masterX) * (180 / Math.PI);
        
        // Calcular punto de inicio de la línea (borde del círculo master)
        const lineStartX = masterX + nodeRadius * Math.cos(angle);
        const lineStartY = masterY + nodeRadius * Math.sin(angle);

        line.style.width = `${lineLength}px`;
        line.style.left = `${lineStartX}px`;
        line.style.top = `${lineStartY}px`;
        line.style.transform = `rotate(${rotation}deg)`;
        line.style.transformOrigin = '0 50%';
        container.appendChild(line);
    });
}

// --- GLOBAL VARIABLES & DATA MANAGEMENT (MODIFIED FOR MOCKING) ---
let networkData = {
    connectedDevices: [],
    availableDevices: [],
    networkStats: {
        status: 'active',
        connectedCount: 0,
        dataRate: 0,
        signalStrength: -30
    }
};

/**
 * NOTE: To disable mock data and use live data from the device,
 * Carga datos reales desde el backend ESP32.
 */
function loadNetworkData() {
    console.log('🔄 Cargando datos en vivo desde ESP32...');
    
    // Cargar información general de la red ESP-NOW
    fetch('/api/esp-now/info')
    .then(r => r.json())
    .then(data => {
        networkData.networkStats.status = data.initialized ? 'active' : 'inactive';
        networkData.networkStats.connectedCount = data.connectedSensors || 0;
        networkData.networkStats.dataRate = data.packetsReceived || 0;
        networkData.networkStats.signalStrength = -42; // Default value, could be enhanced
        updateNetworkStats();
    })
    .catch(e => {
        console.error('Error cargando datos de red:', e);
        networkData.networkStats.status = 'inactive';
        updateNetworkStats();
    });

    // Cargar datos de sensores conectados
    fetch('/multi-sensor-data')
    .then(r => r.json())
    .then(data => {
        console.log('📡 Datos recibidos del servidor:', data);
        
        networkData.connectedDevices = (data.sensors || []).map(sensor => ({
            id: sensor.id,
            name: sensor.name || `Sensor ${sensor.id}`,
            mac: sensor.macAddress || 'Unknown',
            rssi: sensor.signal || -50,
            liquidLevel: sensor.liquidLevel || 0, // Nivel de liquido en cm o %
            distance: sensor.distance || 0, // Distancia del sensor en cm
            liters: sensor.liters || 0, // Litros calculados del backend
            timestamp: sensor.timestamp || 0, // Timestamp real del ESP32 (millis o NTP)
            signal: sensor.signal || 0,
            status: 'online',
            lastSeen: sensor.timestamp ? (sensor.timestamp > 1000000000 ? new Date(sensor.timestamp * 1000).toLocaleTimeString() : `${Math.floor(sensor.timestamp / 1000)}s desde boot`) : 'Ahora',
            peers: ['4'] // Current device as hub
        }));
        
        console.log(`📊 Dispositivos procesados: ${networkData.connectedDevices.length}`);
        
        // Si no hay dispositivos conectados, mostrar mensaje informativo
        if (networkData.connectedDevices.length === 0) {
            console.log('ℹ️ No hay dispositivos ESP-NOW conectados - mostrando estado vacío');
        }
        
        renderDeviceLists();
        renderNetworkTopology();
    })
    .catch(e => {
        console.error('❌ Error cargando dispositivos:', e);
        console.log('🔄 Estableciendo lista vacía como fallback');
        networkData.connectedDevices = [];
        renderDeviceLists();
        renderNetworkTopology();
    });
}

// Función useMockData eliminada - ahora usa datos reales del ESP32

function updateNetworkStats() {
    const statusEl = document.getElementById('network-status');
    const countEl = document.getElementById('connected-count');
    const rateEl = document.getElementById('data-rate');
    const strengthEl = document.getElementById('signal-strength');

    if (statusEl) statusEl.textContent = networkData.networkStats.status === 'active' ? getTranslation('active') : getTranslation('inactive');
    if (countEl) countEl.textContent = networkData.connectedDevices.length;
    if (rateEl) rateEl.textContent = networkData.networkStats.dataRate;
    
    // Show dash if no devices connected, otherwise show average signal
    if (strengthEl) {
        if (networkData.connectedDevices.length === 0) {
            strengthEl.textContent = '-';
        } else {
            // Calculate average signal strength from connected devices
            const avgSignal = networkData.connectedDevices.reduce((sum, device) => sum + (device.signal || device.rssi || -50), 0) / networkData.connectedDevices.length;
            strengthEl.textContent = Math.round(avgSignal) + ' dBm';
        }
    }
}

function renderDeviceLists() {
    renderConnectedDevices();
    renderAvailableDevices();
}

function renderConnectedDevices() {
    const container = document.getElementById('connected-devices');
    if (!container) return;
    container.innerHTML = '';
    if (networkData.connectedDevices.length === 0) {
        container.innerHTML = `<div class="empty-state"><p class="empty-state-message">${getTranslation('noConnectedDevices')}</p></div>`;
        return;
    }
    networkData.connectedDevices.forEach(device => {
        const el = document.createElement('div');
        el.className = 'device-item';
        el.innerHTML = `
            <div class="device-info">
                <div class="device-name">${device.name || `Sensor ${device.id}`}</div>
                <div class="device-details">ID: ${device.id} | Nivel: ${device.liquidLevel || 'N/A'}% | Litros: ${device.liters || 'N/A'}L</div>
                <div class="device-status"><span class="status-dot status-${device.status || 'online'}"></span>${device.status === 'online' ? 'Online' : 'Offline'} - Señal: ${device.rssi || '-?'} dBm | Visto: ${device.lastSeen || 'Nunca'}</div>
            </div>
            <div class="device-actions">
                <button class="action-btn-small" title="${getTranslation('ping')}" onclick="pingDevice('${device.mac}')">
                    <span class="material-symbols-outlined" style="font-size: 0.9rem; margin-right: 4px;">network_ping</span>${getTranslation('ping')}
                </button>
                <button class="action-btn-small danger-btn" title="${getTranslation('disconnect')}" onclick="disconnectDevice('${device.mac}')">
                    <span class="material-symbols-outlined" style="font-size: 0.9rem; margin-right: 4px;">link_off</span>${getTranslation('disconnect')}
                </button>
            </div>
        `;
        container.appendChild(el);
    });
}

function renderAvailableDevices() {
    const container = document.getElementById('available-devices');
    if (!container) return;
    container.innerHTML = '';
    if (networkData.availableDevices.length === 0) {
        container.innerHTML = `<div class="empty-state"><p class="empty-state-message">${getTranslation('noAvailableDevices')}</p></div>`;
        return;
    }
    networkData.availableDevices.forEach(device => {
        const el = document.createElement('div');
        el.className = 'device-item';
        el.innerHTML = `
            <div class="device-info">
                <div class="device-name">${device.name || 'ESP32 Detectado'}</div>
                <div class="device-details">MAC: ${device.mac} | Señal: ${device.rssi} dBm</div>
                <div class="device-status"><span class="status-dot status-offline"></span>Disponible</div>
            </div>
            <div class="device-actions">
                <button class="action-btn-small connect-btn" title="${getTranslation('connect')}" onclick="connectDevice('${device.mac}')">
                    <span class="material-symbols-outlined" style="font-size: 0.9rem; margin-right: 4px;">link</span>${getTranslation('connect')}
                </button>
            </div>
        `;
        container.appendChild(el);
    });
}

function scanForDevices() {
    showToast(getTranslation('scanningNetwork'), 'info');
    const container = document.getElementById('available-devices');
    if(container) container.innerHTML = `<div class="scanning-indicator"><div class="loading-spinner"></div><span>${getTranslation('scanning')}</span></div>`;
    
    fetch('/api/esp-now/scan', { method: 'POST' })
        .then(r => r.json()).then(data => {
            networkData.availableDevices = data.devices || [];
            renderAvailableDevices();
            showToast(`${getTranslation('scanCompleted')} ${networkData.availableDevices.length} ${getTranslation('availableDevices').toLowerCase()}.`, 'success');
        }).catch(e => {
            showToast(getTranslation('scanError'), 'error');
            renderAvailableDevices();
        });
}

function connectDevice(mac) {
    showToast(`🔗 Conectando a ${mac}...`, 'info');
    fetch('/api/esp-now/connect', { method: 'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({mac}) })
        .then(r => r.json()).then(data => {
            if (data.success) {
                showToast('✅ Dispositivo conectado', 'success');
                loadNetworkData();
            } else { showToast('❌ Error al conectar', 'error'); }
        }).catch(e => showToast('❌ Error de red', 'error'));
}

function disconnectDevice(mac) {
    if (!confirm('¿Desconectar este dispositivo?')) return;
    showToast(`🔌 Desconectando ${mac}...`, 'warning');
    fetch('/api/esp-now/disconnect', { method: 'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({mac}) })
        .then(r => r.json()).then(data => {
            if (data.success) {
                showToast('✅ Dispositivo desconectado', 'success');
                loadNetworkData();
            } else { showToast('❌ Error al desconectar', 'error'); }
        }).catch(e => showToast('❌ Error de red', 'error'));
}

function pingDevice(mac) {
    showToast(`🏓 Haciendo ping a ${mac}...`, 'info');
    fetch('/api/esp-now/ping', { method: 'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({mac}) })
        .then(r => r.json()).then(data => {
            if (data.success) {
                showToast(`✅ Ping OK (${data.responseTime}ms)`, 'success');
            } else { showToast('❌ Ping falló', 'error'); }
        }).catch(e => showToast('❌ Error de red', 'error'));
}

function startDataPolling() {
    // This is commented out when using mock data to prevent unnecessary re-renders.
    // setInterval(loadNetworkData, 5000);
}

function showToast(message, type = 'info') {
    const container = document.getElementById('toast-container');
    const toast = document.createElement('div');
    toast.className = `toast toast-${type}`;
    toast.innerHTML = message;
    container.appendChild(toast);
    setTimeout(() => {
        toast.classList.add('show');
        setTimeout(() => {
            toast.classList.remove('show');
            setTimeout(() => container.removeChild(toast), 500);
        }, 3000);
    }, 100);
}

function getLocalIP() { return '192.168.4.1'; }
function getMacAddress() { return 'AA:BB:CC:DD:EE:FF'; }

// Funciones para los botones del panel de control
function refreshNetworkData() {
    console.log('🔄 Refrescando datos de la red...');
    showToast('Actualizando datos de la red...', 'info');
    loadNetworkData();
}

function requestSensorData() {
    console.log('📡 Solicitando datos de sensores...');
    showToast('Solicitando datos de todos los sensores...', 'info');
    
    fetch('/multi-sensor-data')
    .then(response => response.json())
    .then(data => {
        if (data.sensors && Array.isArray(data.sensors)) {
            networkData.connectedDevices = data.sensors;
            renderDeviceLists();
            renderNetworkTopology();
            updateNetworkStats();
            showToast(`Datos actualizados: ${data.sensors.length} sensores`, 'success');
        } else {
            showToast('No se encontraron sensores', 'warning');
        }
    })
    .catch(error => {
        console.error('Error:', error);
        showToast('Error al solicitar datos de sensores', 'error');
    });
}

function resetESPNowNetwork() {
    console.log('🔄 Reiniciando red ESP-NOW...');
    showToast('Reiniciando red ESP-NOW...', 'warning');
    
    fetch('/api/esp-now/reset', { method: 'POST' })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showToast('Red ESP-NOW reiniciada correctamente', 'success');
            // Esperar un momento y recargar datos
            setTimeout(() => {
                loadNetworkData();
            }, 2000);
        } else {
            showToast('Error al reiniciar la red', 'error');
        }
    })
    .catch(error => {
        console.error('Error:', error);
        showToast('Error al reiniciar la red ESP-NOW', 'error');
    });
}

// === SISTEMA DE IDIOMAS ===
const translations = {
    es: {
        // Header
        title: "ESP-NOW Network Manager",
        subtitle: "Gestor de red mesh ESP-NOW",
        
        // Master node
        masterNode: "Dispositivo Principal",
        centralHub: "Hub Central",
        
        // Stats
        networkStatus: "Estado de Red",
        deviceCount: "Dispositivos Conectados", 
        dataRate: "Mensajes/min",
        signalStrength: "Señal Promedio",
        
        // Sections
        networkTopology: "Topología de Red",
        connectedDevices: "Dispositivos Conectados",
        availableDevices: "Dispositivos Disponibles",
        controlPanel: "Panel de Control",
        
        // Buttons
        searchDevices: "Buscar Dispositivos",
        connect: "Conectar",
        disconnect: "Desconectar",
        ping: "Ping",
        refreshNetwork: "Actualizar Red",
        requestData: "Solicitar Datos",
        resetEspNow: "Reiniciar ESP-NOW",
        
        // Descriptions
        refreshDesc: "Refresca los datos de todos los sensores conectados",
        requestDesc: "Fuerza una actualización de datos desde todos los sensores",
        resetDesc: "Reinicia completamente la red ESP-NOW",
        
        // Status
        active: "Activo",
        inactive: "Inactivo",
        online: "Online",
        offline: "Offline",
        
        // Messages
        noConnectedDevices: "No hay dispositivos conectados.",
        noAvailableDevices: "No se encontraron dispositivos.",
        waitingNetworkData: "Esperando datos de la red...",
        scanning: "Escaneando...",
        noSlaveDevices: "No hay sensores esclavos conectados",
        
        // Notifications
        scanningNetwork: "🔍 Escaneando red...",
        scanCompleted: "✅ Escaneo finalizado.",
        deviceConnected: "✅ Dispositivo conectado",
        deviceDisconnected: "✅ Dispositivo desconectado",
        pingOk: "✅ Ping OK",
        networkUpdated: "Datos actualizados:",
        networkReset: "Red ESP-NOW reiniciada correctamente",
        
        // Errors
        scanError: "❌ Error en escaneo",
        connectError: "❌ Error al conectar",
        disconnectError: "❌ Error al desconectar", 
        pingError: "❌ Ping falló",
        networkError: "❌ Error de red",
        resetError: "❌ Error al reiniciar la red ESP-NOW"
    },
    
    en: {
        // Header
        title: "ESP-NOW Network Manager",
        subtitle: "ESP-NOW Mesh Network Manager",
        
        // Master node
        masterNode: "Master Device",
        centralHub: "Central Hub",
        
        // Stats
        networkStatus: "Network Status",
        deviceCount: "Connected Devices",
        dataRate: "Messages/min", 
        signalStrength: "Average Signal",
        
        // Sections
        networkTopology: "Network Topology",
        connectedDevices: "Connected Devices",
        availableDevices: "Available Devices", 
        controlPanel: "Control Panel",
        
        // Buttons
        searchDevices: "Search Devices",
        connect: "Connect",
        disconnect: "Disconnect",
        ping: "Ping",
        refreshNetwork: "Refresh Network",
        requestData: "Request Data",
        resetEspNow: "Reset ESP-NOW",
        
        // Descriptions
        refreshDesc: "Refreshes data from all connected sensors",
        requestDesc: "Forces a data update from all sensors",
        resetDesc: "Completely resets the ESP-NOW network",
        
        // Status
        active: "Active",
        inactive: "Inactive", 
        online: "Online",
        offline: "Offline",
        
        // Messages
        noConnectedDevices: "No connected devices.",
        noAvailableDevices: "No devices found.",
        waitingNetworkData: "Waiting for network data...",
        scanning: "Scanning...",
        noSlaveDevices: "No slave sensors connected",
        noSlaveDevices: "No slave sensors connected",
        
        // Notifications
        scanningNetwork: "🔍 Scanning network...",
        scanCompleted: "✅ Scan completed.",
        deviceConnected: "✅ Device connected",
        deviceDisconnected: "✅ Device disconnected",
        pingOk: "✅ Ping OK",
        networkUpdated: "Data updated:",
        networkReset: "ESP-NOW network reset successfully",
        
        // Errors
        scanError: "❌ Scan error",
        connectError: "❌ Connection error",
        disconnectError: "❌ Disconnection error",
        pingError: "❌ Ping failed", 
        networkError: "❌ Network error",
        resetError: "❌ Error resetting ESP-NOW network"
    },
    
    fr: {
        // Header
        title: "ESP-NOW Network Manager",
        subtitle: "Gestionnaire de réseau maillé ESP-NOW",
        
        // Master node
        masterNode: "Dispositif Maître",
        centralHub: "Hub Central",
        
        // Master node
        masterNode: "Dispositif Maître",
        centralHub: "Hub Central",
        
        // Stats
        networkStatus: "État du Réseau",
        deviceCount: "Appareils Connectés",
        dataRate: "Messages/min",
        signalStrength: "Signal Moyen",
        
        // Sections
        networkTopology: "Topologie du Réseau",
        connectedDevices: "Appareils Connectés",
        availableDevices: "Appareils Disponibles",
        controlPanel: "Panneau de Contrôle",
        
        // Buttons
        searchDevices: "Rechercher Appareils",
        connect: "Connecter",
        disconnect: "Déconnecter",
        ping: "Ping",
        refreshNetwork: "Actualiser Réseau",
        requestData: "Demander Données",
        resetEspNow: "Réinitialiser ESP-NOW",
        
        // Descriptions
        refreshDesc: "Actualise les données de tous les capteurs connectés",
        requestDesc: "Force une mise à jour des données de tous les capteurs",
        resetDesc: "Réinitialise complètement le réseau ESP-NOW",
        
        // Status
        active: "Actif",
        inactive: "Inactif",
        online: "En ligne",
        offline: "Hors ligne",
        
        // Messages
        noConnectedDevices: "Aucun appareil connecté.",
        noAvailableDevices: "Aucun appareil trouvé.",  
        waitingNetworkData: "En attente des données réseau...",
        scanning: "Recherche...",
        noSlaveDevices: "Aucun capteur esclave connecté",
        noSlaveDevices: "Aucun capteur esclave connecté",
        
        // Notifications
        scanningNetwork: "🔍 Recherche du réseau...",
        scanCompleted: "✅ Recherche terminée.",
        deviceConnected: "✅ Appareil connecté",
        deviceDisconnected: "✅ Appareil déconnecté",
        pingOk: "✅ Ping OK",
        networkUpdated: "Données mises à jour:",
        networkReset: "Réseau ESP-NOW réinitialisé avec succès",
        
        // Errors
        scanError: "❌ Erreur de recherche",
        connectError: "❌ Erreur de connexion",
        disconnectError: "❌ Erreur de déconnexion",
        pingError: "❌ Ping échoué",
        networkError: "❌ Erreur réseau",
        resetError: "❌ Erreur lors de la réinitialisation du réseau ESP-NOW"
    }
};

let currentLanguage = 'es';

// Banderas SVG con máscara circular
const flags = {
    es: `<defs>
           <clipPath id="circle-clip"><circle cx="12" cy="12" r="11"/></clipPath>
         </defs>
         <g clip-path="url(#circle-clip)">
           <rect width="8" height="24" fill="#006341"/>
           <rect x="8" width="8" height="24" fill="#ffffff"/>
           <rect x="16" width="8" height="24" fill="#ce1126"/>
           <g transform="translate(12,12)">
             <circle r="2.2" fill="#8B4513" stroke="#FFD700" stroke-width="0.3"/>
             <path d="M-1.2,-0.8 L1.2,-0.8 L0,1.2 Z" fill="#FFD700"/>
             <circle cx="0" cy="-0.3" r="0.25" fill="#8B4513"/>
           </g>
         </g>`,
    en: `<defs>
           <clipPath id="circle-clip-en"><circle cx="12" cy="12" r="11"/></clipPath>
         </defs>
         <g clip-path="url(#circle-clip-en)">
           <rect width="24" height="24" fill="#B22234"/>
           <g>
             <rect y="0" width="24" height="1.8" fill="#FFFFFF"/>
             <rect y="3.6" width="24" height="1.8" fill="#FFFFFF"/>
             <rect y="7.2" width="24" height="1.8" fill="#FFFFFF"/>
             <rect y="10.8" width="24" height="1.8" fill="#FFFFFF"/>
             <rect y="14.4" width="24" height="1.8" fill="#FFFFFF"/>
             <rect y="18" width="24" height="1.8" fill="#FFFFFF"/>
             <rect y="21.6" width="24" height="1.8" fill="#FFFFFF"/>
           </g>
           <rect width="10" height="13" fill="#3C3B6E"/>
           <g fill="#FFFFFF">
             <circle cx="1.5" cy="1.5" r="0.3"/>
             <circle cx="3" cy="1.5" r="0.3"/>
             <circle cx="4.5" cy="1.5" r="0.3"/>
             <circle cx="6" cy="1.5" r="0.3"/>
             <circle cx="7.5" cy="1.5" r="0.3"/>
             <circle cx="9" cy="1.5" r="0.3"/>
             <circle cx="2.25" cy="3" r="0.3"/>
             <circle cx="3.75" cy="3" r="0.3"/>
             <circle cx="5.25" cy="3" r="0.3"/>
             <circle cx="6.75" cy="3" r="0.3"/>
             <circle cx="8.25" cy="3" r="0.3"/>
           </g>
         </g>`,
    fr: `<defs>
           <clipPath id="circle-clip-fr"><circle cx="12" cy="12" r="11"/></clipPath>
         </defs>
         <g clip-path="url(#circle-clip-fr)">
           <rect width="8" height="24" fill="#002395"/>
           <rect x="8" width="8" height="24" fill="#ffffff"/>
           <rect x="16" width="8" height="24" fill="#ED2939"/>
         </g>`
};

function initializeLanguage() {
    // Cargar idioma guardado
    fetch('/api/config/language')
    .then(r => r.json())
    .then(data => {
        currentLanguage = data.language || 'es';
        updateLanguage();
        updateFlag();
    })
    .catch(e => {
        console.log('Usando idioma por defecto: español');
        currentLanguage = 'es';
        updateLanguage();
        updateFlag();
    });
}

let showingLanguageDropdown = false;

function toggleLanguage(event) {
    // Prevenir propagación del evento
    if (event) {
        event.stopPropagation();
        event.preventDefault();
    }
    
    if (showingLanguageDropdown) {
        hideLanguageDropdown();
        return;
    }
    
    showLanguageDropdown();
}

function showLanguageDropdown() {
    hideLanguageDropdown(); // Hide any existing dropdown
    
    const button = document.getElementById('language-toggle');
    if (!button) return;
    
    const dropdown = document.createElement('div');
    dropdown.id = 'language-dropdown';
    dropdown.className = 'language-dropdown';
    
    const languages = [
        { code: 'es', name: 'Español' },
        { code: 'en', name: 'English' },
        { code: 'fr', name: 'Français' }
    ];
    
    // Solo mostrar idiomas diferentes al actual
    const availableLanguages = languages.filter(lang => lang.code !== currentLanguage);
    
    availableLanguages.forEach(lang => {
        const option = document.createElement('div');
        option.className = 'language-option';
        
        // Agregar tooltip informativo
        const tooltipText = {
            'es': 'Cambiar a Español',
            'en': 'Change to English',
            'fr': 'Changer en Français'
        };
        option.title = tooltipText[lang.code];
        
        option.innerHTML = `
            <svg width="50" height="50" viewBox="0 0 24 24" class="flag-icon-small">
                ${flags[lang.code]}
            </svg>
            <span>${lang.name}</span>
        `;
        
        option.onclick = (e) => {
            e.stopPropagation();
            selectLanguage(lang.code);
        };
        dropdown.appendChild(option);
    });
    
    document.body.appendChild(dropdown);
    showingLanguageDropdown = true;
    
    // Position dropdown to the left of the language button
    const buttonRect = button.getBoundingClientRect();
    const isMobile = window.innerWidth <= 768;
    
    dropdown.style.position = 'fixed';
    dropdown.style.zIndex = '1001';
    
    if (isMobile) {
        // En móvil, posición moderada sin sobreponerse y alineada verticalmente
        const leftPosition = Math.max(10, buttonRect.left - 120);
        dropdown.style.left = leftPosition + 'px';
        dropdown.style.top = buttonRect.top + 'px';
    } else {
        // En desktop, posición normal y alineada verticalmente
        dropdown.style.left = (buttonRect.left - 150) + 'px';
        dropdown.style.top = buttonRect.top + 'px';
        
        // Si se sale por la izquierda, ponerlo a la derecha del botón
        if (buttonRect.left - 150 < 0) {
            dropdown.style.left = (buttonRect.right + 10) + 'px';
        }
    }

}

function hideLanguageDropdown() {
    const dropdown = document.getElementById('language-dropdown');
    if (dropdown) {
        dropdown.remove();
        showingLanguageDropdown = false;
    }
}

function selectLanguage(langCode) {
    if (langCode === currentLanguage) {
        hideLanguageDropdown();
        return;
    }
    
    currentLanguage = langCode;
    
    // Actualizar inmediatamente (offline first)
    updateLanguage();
    updateFlag();
    hideLanguageDropdown();
    showToast(`🌍 ${getTranslation('title')}`, 'success');
    
    // Guardar idioma en el servidor
    fetch('/api/config/language', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({language: currentLanguage})
    })
    .then(r => r.json())
    .then(data => {
        // Idioma guardado correctamente
    })
    .catch(e => {
        // Error guardando (funciona offline)
    });
}

// Hide dropdown when clicking outside
setTimeout(() => {
    document.addEventListener('click', (e) => {
        if (showingLanguageDropdown && !e.target.closest('#language-toggle') && !e.target.closest('#language-dropdown')) {
            hideLanguageDropdown();
        }
    });
}, 100);

function updateFlag() {
    const flagIcon = document.getElementById('flag-icon');
    if (flagIcon) {
        flagIcon.innerHTML = flags[currentLanguage];
    }
}

function getTranslation(key) {
    return translations[currentLanguage][key] || translations['es'][key] || key;
}

function updateLanguage() {
    // Actualizar título y subtítulo
    const title = document.querySelector('h1');
    const subtitle = document.querySelector('.espnow-header p');
    if (title) title.childNodes[2].textContent = getTranslation('title');
    if (subtitle) subtitle.textContent = getTranslation('subtitle');
    
    // Actualizar labels de estadísticas
    const statLabels = document.querySelectorAll('.stat-label');
    const statKeys = ['networkStatus', 'deviceCount', 'dataRate', 'signalStrength'];
    statLabels.forEach((label, index) => {
        if (statKeys[index]) {
            label.textContent = getTranslation(statKeys[index]);
        }
    });
    
    // Actualizar títulos de secciones
    const sectionTitles = document.querySelectorAll('h2, h3');
    sectionTitles.forEach(title => {
        const text = title.textContent.trim();
        if (text.includes('Topología') || text.includes('Topology') || text.includes('Topologie')) {
            title.childNodes[1].textContent = ' ' + getTranslation('networkTopology');
        } else if (text.includes('Conectados') || text.includes('Connected') || text.includes('Connectés')) {
            title.childNodes[1].textContent = ' ' + getTranslation('connectedDevices');
        } else if (text.includes('Disponibles') || text.includes('Available') || text.includes('Disponibles')) {
            title.childNodes[1].textContent = ' ' + getTranslation('availableDevices');
        } else if (text.includes('Panel') || text.includes('Control') || text.includes('Panneau')) {
            title.childNodes[1].textContent = ' ' + getTranslation('controlPanel');
        }
    });
    
    // Actualizar botones del panel de control
    const controlButtons = document.querySelectorAll('.control-btn p');
    controlButtons.forEach(btn => {
        const text = btn.textContent.trim();
        if (text.includes('Actualizar') || text.includes('Refresh') || text.includes('Actualiser')) {
            btn.textContent = getTranslation('refreshNetwork');
        } else if (text.includes('Solicitar') || text.includes('Request') || text.includes('Demander')) {
            btn.textContent = getTranslation('requestData');
        } else if (text.includes('Reiniciar') || text.includes('Reset') || text.includes('Réinitialiser')) {
            btn.textContent = getTranslation('resetEspNow');
        }
    });
    
    // Actualizar descripciones
    const descriptions = document.querySelectorAll('.control-description');
    descriptions.forEach(desc => {
        const text = desc.textContent.trim();
        if (text.includes('Refresca') || text.includes('Refreshes') || text.includes('Actualise')) {
            desc.textContent = getTranslation('refreshDesc');
        } else if (text.includes('Fuerza') || text.includes('Forces') || text.includes('Force')) {
            desc.textContent = getTranslation('requestDesc');
        } else if (text.includes('completamente') || text.includes('Completely') || text.includes('complètement')) {
            desc.textContent = getTranslation('resetDesc');
        }
    });
    
    // Re-renderizar listas para aplicar traducciones
    renderDeviceLists();
    renderNetworkTopology();
}

// Global exports for onclick handlers
window.refreshNetworkData = refreshNetworkData;
window.requestSensorData = requestSensorData;
window.resetESPNowNetwork = resetESPNowNetwork;
window.connectDevice = connectDevice;
window.pingDevice = pingDevice;
window.disconnectDevice = disconnectDevice;
window.toggleLanguage = toggleLanguage;