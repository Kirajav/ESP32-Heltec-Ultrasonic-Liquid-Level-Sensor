#include "ESPNowManager.h"
#include "UnifiedLogger.h"
#include <esp_wifi.h>

// Definición de variables estáticas
ESPNowManager* ESPNowManager::instance = nullptr;
std::vector<SensorData> ESPNowManager::connectedSensors;
std::vector<std::array<uint8_t, 6>> ESPNowManager::knownPeers;
std::vector<DiscoveredDevice> ESPNowManager::discoveredDevices;
std::vector<ESPNowManager::SensorMacMapping> ESPNowManager::sensorMacMappings;

// Singleton
ESPNowManager* ESPNowManager::getInstance() {
    if (instance == nullptr) {
        instance = new ESPNowManager();
    }
    return instance;
}

// Inicialización
bool ESPNowManager::initESPNow() {
    if (isInitialized) {
        UnifiedLogger::getInstance().warning("ESP-NOW ya inicializado", "ESP-NOW");
        return true;
    }
    
    UnifiedLogger::getInstance().info("Inicializando ESP-NOW...", "ESP-NOW");
    
    // ESP-NOW funciona con cualquier modo WiFi activo (STA, AP, AP_STA)
    // No forzar AP_STA para evitar reactivar el SoftAP inadvertidamente
    if (WiFi.getMode() == WIFI_MODE_NULL) {
        WiFi.mode(WIFI_STA);
    }
    
    if (esp_now_init() != ESP_OK) {
        UnifiedLogger::getInstance().error("Error inicializando ESP-NOW", "ESP-NOW");
        return false;
    }
    
    esp_now_register_recv_cb(onDataReceived);
    esp_now_register_send_cb(onDataSent);
    
    isInitialized = true;
    packetsSent = 0;
    packetsReceived = 0;
    
    UnifiedLogger::getInstance().info("ESP-NOW inicializado correctamente", "ESP-NOW");
    UnifiedLogger::getInstance().info("MAC Address: " + WiFi.macAddress(), "ESP-NOW");
    
    return true;
}

void ESPNowManager::deinitESPNow() {
    if (!isInitialized) return;
    esp_now_deinit();
    isInitialized = false;
    UnifiedLogger::getInstance().info("ESP-NOW desinicializado", "ESP-NOW");
}

// Callbacks
void ESPNowManager::onDataReceived(const uint8_t *mac, const uint8_t *data, int len) {
    ESPNowManager* mgr = getInstance();
    mgr->packetsReceived++;
    
    // Registrar peer descubierto
    bool isNew = true;
    for (const auto& d : discoveredDevices) {
        if (memcmp(d.mac, mac, 6) == 0) { isNew = false; break; }
    }
    if (isNew) {
        DiscoveredDevice nd;
        memcpy(nd.mac, mac, 6);
        nd.rssi = 0;               // RSSI no disponible en old callback API
        nd.channel = WiFi.channel();
        discoveredDevices.push_back(nd);
        UnifiedLogger::getInstance().info("\xf0\x9f\x93\xa1 Nuevo peer: " + mgr->macToString(mac), "ESP-NOW");
    }

    // Despachar segun tamano del payload
    if (len == sizeof(SensorData)) {
        SensorData sd;
        memcpy(&sd, data, sizeof(SensorData));
        mgr->processSensorData(sd, mac);
    } else if (len == sizeof(SensorCommand)) {
        SensorCommand cmd;
        memcpy(&cmd, data, sizeof(SensorCommand));
        mgr->processCommand(cmd, mac);
    } else if (len == sizeof(TimeSyncData)) {
        TimeSyncData tsd;
        memcpy(&tsd, data, sizeof(TimeSyncData));
        mgr->processTimeSync(tsd, mac);
    }
}

void ESPNowManager::onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    ESPNowManager* mgr = getInstance();
    if (mgr->onSendCallback) {
        mgr->onSendCallback(mac_addr, status);
    }
}

// Procesamiento de datos
void ESPNowManager::processSensorData(const SensorData& data, const uint8_t* mac) {
    SensorData localData = data;
    localData.timestamp = millis(); // Usar tiempo local para tracking de antiguedad
    
    bool found = false;
    for (auto& sensor : connectedSensors) {
        if (sensor.sensorId == localData.sensorId) {
            sensor = localData;
            found = true;
            break;
        }
    }
    if (!found) {
        connectedSensors.push_back(localData);
    }
    
    // Mapear sensorId a MAC
    bool macMapped = false;
    for (auto& m : sensorMacMappings) {
        if (m.sensorId == localData.sensorId) {
            memcpy(m.mac, mac, 6);
            macMapped = true;
            break;
        }
    }
    if (!macMapped) {
        SensorMacMapping mapping;
        mapping.sensorId = localData.sensorId;
        memcpy(mapping.mac, mac, 6);
        sensorMacMappings.push_back(mapping);
    }
    
    if (!isPeerKnown(mac)) {
        addKnownPeer(mac);
        addPeer(mac);
    }
    if (onSensorDataCallback) {
        onSensorDataCallback(localData, mac);
    }
}

void ESPNowManager::processCommand(const SensorCommand& cmd, const uint8_t* mac) {
    UnifiedLogger::getInstance().info("CMD tipo=" + String(cmd.commandType) + " de " + macToString(mac), "ESP-NOW");
    
    switch (cmd.commandType) {
        case CMD_PING: {
            // Responder con pong si es request (value=0)
            if (cmd.commandValue == 0) {
                SensorCommand pong;
                pong.targetSensorId = cmd.targetSensorId;
                pong.commandType = CMD_PING;
                pong.commandValue = 1; // 1 = pong response
                pong.timestamp = millis();
                sendCommand(mac, pong);
            }
            break;
        }
        case CMD_REQUEST_DATA:
            // Solicitud de datos: callback de la app se encargara
            break;
        case CMD_TIME_SYNC:
            // Solicitud de time sync: master responde via callback
            break;
        default:
            break;
    }
    
    if (!isPeerKnown(mac)) {
        addKnownPeer(mac);
        addPeer(mac);
    }
}

// Gestión de Peers
bool ESPNowManager::addPeer(const uint8_t* macAddress) {
    if (!isInitialized || esp_now_is_peer_exist(macAddress)) return false;
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, macAddress, 6);
    peerInfo.channel = 0; // 0 para usar el canal actual
    peerInfo.encrypt = false;
    esp_err_t result = esp_now_add_peer(&peerInfo);
    if (result == ESP_OK) {
        addKnownPeer(macAddress);
        UnifiedLogger::getInstance().info("Peer agregado: " + macToString(macAddress), "ESP-NOW");
        return true;
    }
    return false;
}

bool ESPNowManager::removePeer(const uint8_t* macAddress) {
    if (!isInitialized || !esp_now_is_peer_exist(macAddress)) return false;
    esp_err_t result = esp_now_del_peer(macAddress);
    if (result == ESP_OK) {
        // Eliminar de la lista de peers conocidos y sensores conectados
        knownPeers.erase(std::remove_if(knownPeers.begin(), knownPeers.end(), [&](const std::array<uint8_t, 6>& p) { return memcmp(p.data(), macAddress, 6) == 0; }), knownPeers.end());
        // Aquí se podría añadir lógica para eliminar el sensor de `connectedSensors` si se identifica por MAC
        UnifiedLogger::getInstance().info("Peer eliminado: " + macToString(macAddress), "ESP-NOW");
        return true;
    }
    return false;
}

// --- Implementación de Nuevas Funciones ---

void ESPNowManager::scanForPeers(uint32_t scanDuration) {
    // Escaneo activo: los callbacks de ESP-NOW no necesitan modo promiscuo
    // (antes se activaba y nunca se apagaba). onDataReceived registra a quien
    // responda; AppManager mantiene la radio despierta y repite broadcastPing().
    UnifiedLogger::getInstance().info("Iniciando escaneo activo de ESP-NOW...", "ESP-NOW");
    discoveredDevices.clear();
    broadcastPing();
}

// PING a FF:FF:FF:FF:FF:FF: cada unidad que lo oiga responde con PONG unicast
// y ambas se agregan como peers (ver processCommand)
bool ESPNowManager::broadcastPing() {
    if (!isInitialized) return false;
    SensorCommand ping;
    ping.targetSensorId = 0;
    ping.commandType = CMD_PING;
    ping.commandValue = 0;  // 0 = request
    ping.timestamp = millis();
    uint8_t broadcastMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (!esp_now_is_peer_exist(broadcastMAC)) {
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, broadcastMAC, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
    }
    esp_err_t result = esp_now_send(broadcastMAC, (const uint8_t*)&ping, sizeof(SensorCommand));
    if (result == ESP_OK) { packetsSent++; return true; }
    return false;
}

String ESPNowManager::getDiscoveredDevicesJSON() {
    DynamicJsonDocument doc(1024);
    JsonArray devices = doc.createNestedArray("devices");
    for (const auto& device : discoveredDevices) {
        JsonObject deviceObj = devices.createNestedObject();
        deviceObj["mac"] = macToString(device.mac);
        deviceObj["rssi"] = device.rssi;
        deviceObj["channel"] = device.channel;
    }
    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
}

bool ESPNowManager::pingPeer(const uint8_t* macAddress) {
    if (!isInitialized) return false;
    // Implementación simplificada: verifica si el peer está en la lista de ESP-NOW.
    // Una implementación real enviaría un paquete PING y esperaría un PONG.
    return esp_now_is_peer_exist(macAddress);
}

// --- Funciones existentes (sin cambios mayores) ---

String ESPNowManager::getSensorsJSON() {
    DynamicJsonDocument doc(2048);
    JsonArray sensorsArray = doc.createNestedArray("sensors");
    for (const auto& sensor : connectedSensors) {
        JsonObject sensorObj = sensorsArray.createNestedObject();
        sensorObj["id"] = sensor.sensorId;
        sensorObj["name"] = sensor.sensorName;
        sensorObj["liquidLevel"] = sensor.liquidLevel;
        sensorObj["distance"] = sensor.distance;
        sensorObj["liters"] = sensor.liters;
        sensorObj["signal"] = sensor.signalStrength;
        sensorObj["timestamp"] = sensor.timestamp;
    }
    doc["count"] = connectedSensors.size();
    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
}

String ESPNowManager::getNetworkStatusJSON() {
    DynamicJsonDocument doc(1024);
    doc["initialized"] = isInitialized;
    doc["isMaster"] = isMaster;
    doc["packetsSent"] = packetsSent;
    doc["packetsReceived"] = packetsReceived;
    doc["connectedSensors"] = connectedSensors.size();
    doc["macAddress"] = WiFi.macAddress();
    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
}

// ... resto de funciones de utilidad como macToString, etc. ...
String ESPNowManager::macToString(const uint8_t* mac) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}

bool ESPNowManager::isPeerKnown(const uint8_t* mac) {
    for (const auto& knownMac : knownPeers) {
        if (memcmp(knownMac.data(), mac, 6) == 0) {
            return true;
        }
    }
    return false;
}

void ESPNowManager::addKnownPeer(const uint8_t* mac) {
    if (!isPeerKnown(mac)) {
        std::array<uint8_t, 6> newMac;
        memcpy(newMac.data(), mac, 6);
        knownPeers.push_back(newMac);
    }
}

uint8_t ESPNowManager::getConnectedSensorCount() {
    return connectedSensors.size();
}

// ===== FUNCIONES DE ENVÍO DE DATOS =====

void ESPNowManager::removeAllPeers() {
    if (!isInitialized) return;
    for (const auto& mac : knownPeers) {
        esp_now_del_peer(mac.data());
    }
    knownPeers.clear();
    connectedSensors.clear();
    sensorMacMappings.clear();
    UnifiedLogger::getInstance().info("Todos los peers eliminados", "ESP-NOW");
}

bool ESPNowManager::broadcastData(const SensorData& data) {
    if (!isInitialized) return false;
    // Broadcast FF:FF:FF:FF:FF:FF llega a todos los dispositivos ESP-NOW en el canal
    uint8_t broadcastMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (!esp_now_is_peer_exist(broadcastMAC)) {
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, broadcastMAC, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
    }
    esp_err_t result = esp_now_send(broadcastMAC, (const uint8_t*)&data, sizeof(SensorData));
    if (result == ESP_OK) {
        packetsSent++;
        return true;
    }
    return false;
}

bool ESPNowManager::sendDataToPeer(const uint8_t* macAddress, const SensorData& data) {
    if (!isInitialized) return false;
    if (!esp_now_is_peer_exist(macAddress)) addPeer(macAddress);
    esp_err_t result = esp_now_send(macAddress, (const uint8_t*)&data, sizeof(SensorData));
    if (result == ESP_OK) { packetsSent++; return true; }
    return false;
}

bool ESPNowManager::sendCommand(const uint8_t* macAddress, const SensorCommand& cmd) {
    if (!isInitialized) return false;
    if (!esp_now_is_peer_exist(macAddress)) addPeer(macAddress);
    esp_err_t result = esp_now_send(macAddress, (const uint8_t*)&cmd, sizeof(SensorCommand));
    if (result == ESP_OK) { packetsSent++; return true; }
    return false;
}

bool ESPNowManager::sendCommandToAll(const SensorCommand& cmd) {
    if (!isInitialized || knownPeers.empty()) return false;
    bool allOk = true;
    for (const auto& mac : knownPeers) {
        if (!sendCommand(mac.data(), cmd)) allOk = false;
    }
    return allOk;
}

bool ESPNowManager::broadcastTimeSync(const TimeSyncData& timeData) {
    if (!isInitialized) return false;
    uint8_t broadcastMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (!esp_now_is_peer_exist(broadcastMAC)) {
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, broadcastMAC, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
    }
    esp_err_t result = esp_now_send(broadcastMAC, (const uint8_t*)&timeData, sizeof(TimeSyncData));
    if (result == ESP_OK) { packetsSent++; return true; }
    return false;
}

bool ESPNowManager::sendTimeSyncToPeer(const uint8_t* macAddress, const TimeSyncData& timeData) {
    if (!isInitialized) return false;
    if (!esp_now_is_peer_exist(macAddress)) addPeer(macAddress);
    esp_err_t result = esp_now_send(macAddress, (const uint8_t*)&timeData, sizeof(TimeSyncData));
    if (result == ESP_OK) { packetsSent++; return true; }
    return false;
}

void ESPNowManager::requestTimeSync() {
    // Esclavo solicita sincronizacion de tiempo al master
    SensorCommand cmd;
    cmd.targetSensorId = 0;
    cmd.commandType = CMD_TIME_SYNC;
    cmd.commandValue = 0;
    cmd.timestamp = millis();
    
    uint8_t broadcastMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (!esp_now_is_peer_exist(broadcastMAC)) {
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, broadcastMAC, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
    }
    esp_now_send(broadcastMAC, (const uint8_t*)&cmd, sizeof(SensorCommand));
}

std::vector<SensorData> ESPNowManager::getConnectedSensors() {
    return connectedSensors;
}

SensorData* ESPNowManager::getSensorById(uint8_t sensorId) {
    for (auto& sensor : connectedSensors) {
        if (sensor.sensorId == sensorId) return &sensor;
    }
    return nullptr;
}

void ESPNowManager::clearOldSensors(uint32_t maxAge) {
    uint32_t now = millis();
    connectedSensors.erase(
        std::remove_if(connectedSensors.begin(), connectedSensors.end(),
            [now, maxAge](const SensorData& s) { return (now - s.timestamp) > maxAge; }),
        connectedSensors.end()
    );
}

String ESPNowManager::getNetworkStatus() {
    String status = isInitialized ? "Activo" : "Inactivo";
    status += " | " + String(isMaster ? "Master" : "Slave");
    status += " | Sensores:" + String(connectedSensors.size());
    status += " | TX:" + String(packetsSent) + " RX:" + String(packetsReceived);
    return status;
}

void ESPNowManager::printMac(const uint8_t* mac) {
    Serial.println(macToString(mac));
}

bool ESPNowManager::stringToMac(const String& macStr, uint8_t* mac) {
    if (macStr.length() < 17) return false;
    return sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) == 6;
}

void ESPNowManager::processTimeSync(const TimeSyncData& data, const uint8_t* mac) {
    UnifiedLogger::getInstance().info("TimeSync de " + macToString(mac), "ESP-NOW");
    if (onTimeSyncCallback) {
        onTimeSyncCallback(data, mac);
    }
}

String ESPNowManager::getMacForSensorId(uint8_t sensorId) {
    for (const auto& mapping : sensorMacMappings) {
        if (mapping.sensorId == sensorId) {
            return macToString(mapping.mac);
        }
    }
    return "00:00:00:00:00:00";
}
