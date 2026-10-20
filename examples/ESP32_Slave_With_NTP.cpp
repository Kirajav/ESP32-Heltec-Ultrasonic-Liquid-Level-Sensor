/*
 * EJEMPLO: ESP32 SLAVE CON CONEXIÓN A INTERNET (NTP)
 * 
 * Este código es para un ESP32 esclavo que:
 * 1. Se conecta a WiFi para obtener hora NTP
 * 2. Envía datos con timestamp real al master (que no tiene internet)
 * 3. Opcionalmente puede sincronizar el master con la hora real
 */

#include <esp_now.h>
#include <WiFi.h>
#include <HCSR04.h>
#include <time.h>
#include "NTPClient.h"
#include "WiFiUdp.h"

// Estructura de datos (debe coincidir con el principal)
struct SensorData {
    uint8_t sensorId;           
    float waterLevel;           // Nivel de agua en cm
    float distance;             // Distancia del sensor en cm
    float liters;              // Litros calculados
    uint32_t timestamp;         // Timestamp (UNIX time si NTP disponible)
    char sensorName[32];        
    uint8_t signalStrength;     
};

// Estructura para sincronizar tiempo al master
struct TimeSyncData {
    uint32_t masterTimestamp;   // Tiempo NTP actual
    uint32_t localTimestamp;    // millis() local
    int8_t timezoneOffset;      // GMT offset
    bool isNTPSynced;          // true si tenemos NTP
    uint8_t signalQuality;     // Calidad de la sincronización
};

// Configuración WiFi y NTP
const char* ssid = "TU_WIFI_SSID";
const char* password = "TU_WIFI_PASSWORD";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -6 * 3600;  // México GMT-6
const int daylightOffset_sec = 3600;   // Horario de verano

// Configuración sensor
#define SENSOR_ID 2
#define TRIG_PIN 4
#define ECHO_PIN 5

// Variables globales
HCSR04 ultrasonicSensor(TRIG_PIN, ECHO_PIN);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec, 60000);
uint8_t masterMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
bool masterFound = false;
bool ntpSynced = false;

// Callback cuando se envían datos
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("📡 Envío ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "✅ Exitoso" : "❌ Fallido");
}

// Callback cuando se reciben datos del master
void onDataReceived(const uint8_t * mac, const uint8_t *incomingData, int len) {
    Serial.println("📨 Comando recibido del master");
    
    // Si el master solicita sincronización de tiempo
    if (len == 1 && incomingData[0] == 0x99) { // Código para solicitar tiempo
        sendTimeSync();
    }
}

// Inicializar WiFi y NTP
void initWiFiAndNTP() {
    Serial.println("📡 Conectando a WiFi...");
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("✅ WiFi conectado");
        Serial.println("IP: " + WiFi.localIP().toString());
        
        // Inicializar NTP
        timeClient.begin();
        timeClient.update();
        
        if (timeClient.isTimeSet()) {
            Serial.println("✅ NTP sincronizado");
            Serial.println("Hora actual: " + timeClient.getFormattedTime());
            ntpSynced = true;
        } else {
            Serial.println("⚠️ NTP no disponible, usando millis()");
        }
    } else {
        Serial.println("❌ WiFi no conectado, usando millis()");
    }
}

// Enviar sincronización de tiempo al master
void sendTimeSync() {
    if (!masterFound || !ntpSynced) return;
    
    TimeSyncData syncData;
    syncData.masterTimestamp = timeClient.getEpochTime();
    syncData.localTimestamp = millis();
    syncData.timezoneOffset = gmtOffset_sec / 3600;
    syncData.isNTPSynced = true;
    syncData.signalQuality = WiFi.RSSI() + 100;
    
    esp_now_send(masterMAC, (uint8_t *) &syncData, sizeof(syncData));
    Serial.println("⏰ Sincronización de tiempo enviada al master");
}

// Obtener timestamp actual
uint32_t getCurrentTimestamp() {
    if (ntpSynced) {
        timeClient.update();
        return timeClient.getEpochTime();
    } else {
        return millis(); // Fallback a millis()
    }
}

// Enviar datos del sensor
void sendSensorData() {
    if (!masterFound) {
        discoverMaster();
        return;
    }
    
    // Leer sensor ultrasonico
    float distance = ultrasonicSensor.dist();
    
    // Calcular nivel de agua y litros
    float tankHeight = 100.0;  // Altura del tanque en cm
    float tankWidth = 100.0;   // Ancho del tanque en cm  
    float tankLength = 80.0;   // Largo del tanque en cm
    
    float waterLevel = tankHeight - distance;
    if (waterLevel < 0) waterLevel = 0;
    
    float liters = (waterLevel * tankWidth * tankLength) / 1000.0;
    
    // Preparar datos
    SensorData data;
    data.sensorId = SENSOR_ID;
    data.waterLevel = waterLevel;
    data.distance = distance;
    data.liters = liters;
    data.timestamp = getCurrentTimestamp(); // ⭐ NTP time o millis()
    strcpy(data.sensorName, "Sensor_Con_NTP");
    data.signalStrength = WiFi.RSSI() + 100;
    
    // Enviar datos
    esp_now_send(masterMAC, (uint8_t *) &data, sizeof(data));
    
    Serial.println("📊 Datos enviados:");
    Serial.println("   💧 Nivel: " + String(waterLevel) + " cm");
    Serial.println("   📏 Distancia: " + String(distance) + " cm");
    Serial.println("   🪣 Litros: " + String(liters) + " L");
    if (ntpSynced) {
        Serial.println("   🕐 Hora NTP: " + timeClient.getFormattedTime());
    } else {
        Serial.println("   ⏰ Millis: " + String(data.timestamp));
    }
}

// Buscar master
void discoverMaster() {
    Serial.println("🔍 Buscando ESP32 principal...");
    
    SensorData discoveryData;
    discoveryData.sensorId = SENSOR_ID;
    discoveryData.timestamp = getCurrentTimestamp();
    strcpy(discoveryData.sensorName, "Sensor_Con_NTP");
    
    uint8_t broadcastMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_send(broadcastMAC, (uint8_t *) &discoveryData, sizeof(discoveryData));
}

void setup() {
    Serial.begin(115200);
    Serial.println("🚀 ESP32 Sensor con NTP Iniciando...");
    
    // Configurar WiFi y NTP
    initWiFiAndNTP();
    
    // Configurar WiFi en modo STA+AP para ESP-NOW
    WiFi.mode(WIFI_AP_STA);
    Serial.println("📱 MAC Address: " + WiFi.macAddress());
    
    // Inicializar ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ Error inicializando ESP-NOW");
        return;
    }
    
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataReceived);
    
    // Configurar peer
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, masterMAC, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("❌ Error agregando peer");
        return;
    }
    
    Serial.println("✅ ESP-NOW inicializado");
}

void loop() {
    // Enviar datos cada 30 segundos
    static unsigned long lastSend = 0;
    if (millis() - lastSend > 30000) {
        sendSensorData();
        lastSend = millis();
    }
    
    // Sincronizar tiempo con master cada 5 minutos
    static unsigned long lastTimeSync = 0;
    if (ntpSynced && masterFound && millis() - lastTimeSync > 300000) {
        sendTimeSync();
        lastTimeSync = millis();
    }
    
    // Mantener conexión WiFi y NTP
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️ WiFi desconectado, reintentando...");
        initWiFiAndNTP();
    }
    
    delay(1000);
}