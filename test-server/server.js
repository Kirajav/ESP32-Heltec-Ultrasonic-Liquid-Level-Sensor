const express = require('express');
const cors = require('cors');
const path = require('path');

const app = express();
const PORT = 3000;

app.use(cors());
app.use(express.json());

const webRoot = path.join(__dirname, '..', 'data', 'web');
app.use(express.static(webRoot));

console.log(`Sirviendo archivos estáticos desde: ${webRoot}`);

// --- Base de Datos Falsa para Topología en Cascada ---
let mockDB = {
    devices: [
        { mac: 'AA:BB:CC:11:22:01', name: 'Sensor 1', role: 'Sensor', online: true, rssi: -45, via: 'Master' },
        { mac: 'AA:BB:CC:11:22:02', name: 'Sensor 2', role: 'Sensor', online: true, rssi: -55, via: 'Sensor 1' },
        { mac: 'AA:BB:CC:11:22:03', name: 'Sensor 3', role: 'Sensor', online: true, rssi: -65, via: 'Sensor 2' },
        { mac: 'AA:BB:CC:11:22:04', name: 'Sensor 4', role: 'Sensor', online: true, rssi: -75, via: 'Sensor 3' },
    ],
    masterInfo: {
        mac: 'M1:M2:M3:M4:M5:M6',
        channel: 6,
        role: 'Master'
    }
};

// --- Endpoints de la API ESP-NOW ---

app.get('/api/esp-now/info', (req, res) => {
    res.json({
        initialized: true,
        status: 'active',
        connectedSensors: 0,  // Sin dispositivos conectados
        packetsReceived: 0
    });
});

app.post('/api/esp-now/scan', (req, res) => {
    setTimeout(() => {
        res.json({ success: true, devices: [] }); // No new devices for this mock
    }, 2000);
});

app.get('/multi-sensor-data', (req, res) => {
    console.log("📡 Endpoint /multi-sensor-data llamado. Sirviendo datos mockeados de esclavo.");
    const mockSlaveSensor = {
        id: "SLAVE_01",
        name: "Cisterna (Esclavo)",
        online: true,
        rssi: -65,
        percentage: 45,
        waterLevel: "2250",
        distance: "110",
        lastUpdate: new Date().toISOString()
    };
    res.json({ sensors: [mockSlaveSensor] });
});

// Otros endpoints para mantener la interactividad
app.post('/api/esp-now/ping', (req, res) => res.json({ success: true, responseTime: Math.floor(Math.random() * 50) + 20 }));
app.post('/api/esp-now/disconnect', (req, res) => res.json({ success: true }));
app.post('/api/esp-now/connect', (req, res) => res.json({ success: true }));
app.post('/api/esp-now/reset', (req, res) => {
    mockDB.devices = [];
    res.json({ success: true });
});

// --- Iniciar Servidor ---
app.listen(PORT, () => {
    console.log(`🚀 Servidor de pruebas iniciado en http://localhost:${PORT}`);
    console.log(`Puedes abrir la página de ESP-NOW en: http://localhost:${PORT}/esp_now/`);
});