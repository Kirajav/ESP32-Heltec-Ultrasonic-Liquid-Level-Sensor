/*
 * EJEMPLO: ESP32 SLAVE PARA CONECTAR AL SISTEMA PRINCIPAL
 * 
 * Este código va en un ESP32 secundario que actuará como sensor remoto
 * Se conecta automáticamente al ESP32 principal vía ESP-NOW
 */

#include <esp_now.h>
#include <WiFi.h>
#include <HCSR04.h>

// Estructura de datos (debe coincidir con el principal)
struct SensorData {
    uint8_t sensorId;           
    float waterLevel;           // Nivel de agua en cm
    float distance;             // Distancia del sensor en cm
    float liters;              // Litros calculados
    uint32_t timestamp;         // Timestamp (millis() o NTP)
    char sensorName[32];        
    uint8_t signalStrength;     
};

// Configuración
#define SENSOR_ID 2  // ID único para este sensor
#define TRIG_PIN 4   // Pin trigger del sensor
#define ECHO_PIN 5   // Pin echo del sensor

// Variables globales
HCSR04 ultrasonicSensor(TRIG_PIN, ECHO_PIN);
uint8_t masterMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // MAC del ESP32 principal (se auto-descubre)
bool masterFound = false;

// Callback cuando se envían datos
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("📡 Envío ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "✅ Exitoso" : "❌ Fallido");
}

// Callback cuando se reciben datos
void onDataReceived(const uint8_t * mac, const uint8_t *incomingData, int len) {
    Serial.println("📨 Comando recibido del master");
    // Aquí se pueden procesar comandos del master
}

// Buscar ESP32 principal (master)
void discoverMaster() {
    Serial.println("🔍 Buscando ESP32 principal...");
    
    // Broadcast para encontrar el master
    SensorData discoveryData;
    discoveryData.sensorId = SENSOR_ID;
    discoveryData.timestamp = millis();
    strcpy(discoveryData.sensorName, "Sensor_Remoto_1");
    
    // Enviar a dirección broadcast
    uint8_t broadcastMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_send(broadcastMAC, (uint8_t *) &discoveryData, sizeof(discoveryData));
}

// Enviar datos del sensor
void sendSensorData() {
    if (!masterFound) {
        discoverMaster();
        return;
    }
    
    // Leer sensor ultrasonico
    float distance = ultrasonicSensor.dist();
    
    // Calcular nivel de agua (personalizar según tu tanque)
    float tankHeight = 100.0;  // Altura del tanque en cm
    float waterLevel = tankHeight - distance;
    if (waterLevel < 0) waterLevel = 0;
    
    // Preparar datos
    SensorData data;
    data.sensorId = SENSOR_ID;
    data.waterLevel = waterLevel;
    data.distance = distance;
    // Calcular litros basado en dimensiones del tanque (personalizar)
    float tankWidth = 100.0;   // Ancho del tanque en cm
    float tankLength = 80.0;   // Largo del tanque en cm
    data.liters = (waterLevel * tankWidth * tankLength) / 1000.0; // cm³ a litros
    
    data.timestamp = millis(); // Tiempo desde boot (o NTP si está sincronizado)
    strcpy(data.sensorName, "Tanque_Remoto");
    data.signalStrength = WiFi.RSSI() + 100; // Convertir a porcentaje
    
    // Enviar datos
    esp_err_t result = esp_now_send(masterMAC, (uint8_t *) &data, sizeof(data));
    
    Serial.println("📊 Datos enviados:");
    Serial.println("   💧 Nivel: " + String(waterLevel) + " cm");
    Serial.println("   📏 Distancia: " + String(distance) + " cm");
    Serial.println("   🪣 Litros: " + String(data.liters) + " L");
    Serial.println("   ⏰ Timestamp: " + String(data.timestamp) + " ms");
}

void setup() {
    Serial.begin(115200);
    Serial.println("🚀 ESP32 Sensor Remoto Iniciando...");
    
    // Configurar WiFi
    WiFi.mode(WIFI_STA);
    Serial.println("📱 MAC Address: " + WiFi.macAddress());
    
    // Inicializar ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ Error inicializando ESP-NOW");
        return;
    }
    
    // Registrar callbacks
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataReceived);
    
    // Configurar peer para comunicación con master
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, masterMAC, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
    
    // Agregar peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("❌ Error agregando peer");
        return;
    }
    
    Serial.println("✅ ESP-NOW inicializado");
    Serial.println("🔍 Buscando ESP32 principal...");
}

void loop() {
    // Enviar datos cada 30 segundos
    static unsigned long lastSend = 0;
    if (millis() - lastSend > 30000) {
        sendSensorData();
        lastSend = millis();
    }
    
    // Buscar master cada 5 minutos si no se ha encontrado
    static unsigned long lastDiscovery = 0;
    if (!masterFound && millis() - lastDiscovery > 300000) {
        discoverMaster();
        lastDiscovery = millis();
    }
    
    delay(1000);
}