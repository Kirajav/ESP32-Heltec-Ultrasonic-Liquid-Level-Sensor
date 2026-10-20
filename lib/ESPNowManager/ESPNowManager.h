#ifndef ESPNOW_MANAGER_H
#define ESPNOW_MANAGER_H

#include <esp_now.h>
#include <WiFi.h>
#include <vector>
#include <array>
#include <ArduinoJson.h>

// Estructura para dispositivos descubiertos en un escaneo
struct DiscoveredDevice {
    uint8_t mac[6];
    int rssi;
    int channel;
};

// Estructura de datos del sensor
struct SensorData {
    uint8_t sensorId;
    float liquidLevel;           // Nivel de líquido en cm o %
    float distance;             // Distancia del sensor en cm
    float liters;              // Litros calculados basado en dimensiones del tanque
    uint32_t timestamp;         // Timestamp (millis() o NTP si disponible)
    char sensorName[32];
    uint8_t signalStrength;
};

// Estructura de comando
struct SensorCommand {
    uint8_t targetSensorId;
    uint8_t commandType;
    uint8_t commandValue;
    uint32_t timestamp;
};

// Estructura para sincronización de tiempo NTP
struct TimeSyncData {
    uint32_t masterTimestamp;
    uint32_t localTimestamp;
    int8_t timezoneOffset;
    bool isNTPSynced;
    uint8_t signalQuality;
};

// Tipos de comando
enum CommandType {
    CMD_PING = 1,
    CMD_REQUEST_DATA = 2,
    CMD_SET_INTERVAL = 3,
    CMD_CALIBRATE = 4,
    CMD_SLEEP = 5,
    CMD_WAKE = 6,
    CMD_RESET = 7,
    CMD_TIME_SYNC = 8
};

class ESPNowManager {
private:
    static ESPNowManager* instance;
    static std::vector<SensorData> connectedSensors;
    static std::vector<std::array<uint8_t, 6>> knownPeers;
    static std::vector<DiscoveredDevice> discoveredDevices; // Para resultados de escaneo
    
    // Mapeo sensorId → MAC para identificar peers
    struct SensorMacMapping {
        uint8_t sensorId;
        uint8_t mac[6];
    };
    static std::vector<SensorMacMapping> sensorMacMappings;
    
    static void onDataReceived(const uint8_t *mac, const uint8_t *data, int len);
    static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
    
    void processSensorData(const SensorData& data, const uint8_t* mac);
    void processCommand(const SensorCommand& cmd, const uint8_t* mac);
    void processTimeSync(const TimeSyncData& data, const uint8_t* mac);
    bool isPeerKnown(const uint8_t* mac);
    void addKnownPeer(const uint8_t* mac);
    
    bool isInitialized = false;
    bool isMaster = true;
    uint32_t lastBroadcast = 0;
    uint16_t packetsSent = 0;
    uint16_t packetsReceived = 0;
    
public:
    static ESPNowManager* getInstance();
    
    bool initESPNow();
    void deinitESPNow();
    bool isInit() { return isInitialized; }
    
    bool addPeer(const uint8_t* macAddress);
    bool removePeer(const uint8_t* macAddress);
    void removeAllPeers();
    
    bool broadcastData(const SensorData& data);
    bool sendDataToPeer(const uint8_t* macAddress, const SensorData& data);
    bool sendCommand(const uint8_t* macAddress, const SensorCommand& cmd);
    bool sendCommandToAll(const SensorCommand& cmd);
    
    bool broadcastTimeSync(const TimeSyncData& timeData);
    bool sendTimeSyncToPeer(const uint8_t* macAddress, const TimeSyncData& timeData);
    void requestTimeSync();
    
    std::vector<SensorData> getConnectedSensors();
    SensorData* getSensorById(uint8_t sensorId);
    void clearOldSensors(uint32_t maxAge = 60000);
    
    uint16_t getPacketsSent() { return packetsSent; }
    uint16_t getPacketsReceived() { return packetsReceived; }
    uint8_t getConnectedSensorCount();
    bool hasPeers() { return !knownPeers.empty(); }
    String getNetworkStatus();
    
    String macToString(const uint8_t* mac);
    void printMac(const uint8_t* mac);
    bool stringToMac(const String& macStr, uint8_t* mac);
    
    String getSensorsJSON();
    String getNetworkStatusJSON();
    
    void setMasterMode(bool master) { isMaster = master; }
    bool isMasterMode() { return isMaster; }

    // --- Nuevas funciones para la Web UI ---
    void scanForPeers(uint32_t scanDuration = 5000);
    bool broadcastPing();
    String getDiscoveredDevicesJSON();
    bool pingPeer(const uint8_t* macAddress);
    
    std::function<void(const SensorData&, const uint8_t*)> onSensorDataCallback;
    std::function<void(const uint8_t*, esp_now_send_status_t)> onSendCallback;
    std::function<void(const TimeSyncData&, const uint8_t*)> onTimeSyncCallback;
    
    String getMacForSensorId(uint8_t sensorId);
};

#endif // ESPNOW_MANAGER_H