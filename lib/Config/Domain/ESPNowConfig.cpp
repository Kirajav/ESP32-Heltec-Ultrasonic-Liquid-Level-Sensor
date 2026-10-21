#include "ESPNowConfig.h"
#include <WiFi.h>
#include <esp_system.h>

static String macToStr(const uint8_t* mac) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

static bool strToMac(const char* s, uint8_t* mac) {
    if (!s || strlen(s) < 17) return false;
    return sscanf(s, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) == 6;
}

ESPNowConfig::ESPNowConfig() {
    // Default values. Automático: sin clave ni unidades emparejadas no hace nada,
    // así que actualizar desde 3.4.x no cambia el comportamiento hasta emparejar
    mode = ESPNOW_MODE_AUTO;
    role = ESPNOW_ROLE_EMISOR;
    lastChannel = 0;
    customSensorID = 0;
    customSensorName = "";
    useAutoID = true;
    useAutoName = true;
}

void ESPNowConfig::initialize() {
    generateAutoIdentity();
}

void ESPNowConfig::generateAutoIdentity() {
    // Get device MAC address
    deviceMAC = WiFi.macAddress();

    // Get chip ID (unique per ESP32)
    uint32_t chipId = 0;
    for (int i = 0; i < 17; i = i + 8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    deviceChipID = String(chipId, HEX);
    deviceChipID.toUpperCase();

    // Generate auto sensor ID from chip ID (1-254 range)
    autoSensorID = (chipId % 254) + 1;

    // Generate auto sensor name
    autoSensorName = "Sensor-" + deviceChipID.substring(0, 6);

    Serial.printf("🆔 Auto-generated identity:\n");
    Serial.printf("   MAC: %s\n", deviceMAC.c_str());
    Serial.printf("   Chip ID: %s\n", deviceChipID.c_str());
    Serial.printf("   Auto Sensor ID: %d\n", autoSensorID);
    Serial.printf("   Auto Sensor Name: %s\n", autoSensorName.c_str());
}

const char* ESPNowConfig::modeToString(EspNowMode m) {
    switch (m) {
        case ESPNOW_MODE_OFF:    return "apagado";
        case ESPNOW_MODE_MANUAL: return "manual";
        default:                 return "automatico";
    }
}

bool ESPNowConfig::modeFromString(const String& s, EspNowMode& out) {
    String v = s;
    v.toLowerCase();
    if (v == "apagado" || v == "off") { out = ESPNOW_MODE_OFF; return true; }
    if (v == "manual" || v == "on") { out = ESPNOW_MODE_MANUAL; return true; }
    if (v == "automatico" || v == "automático" || v == "auto") { out = ESPNOW_MODE_AUTO; return true; }
    return false;
}

const char* ESPNowConfig::roleToString(EspNowRole r) {
    return r == ESPNOW_ROLE_RECEPTOR ? "receptor" : "emisor";
}

bool ESPNowConfig::roleFromString(const String& s, EspNowRole& out) {
    String v = s;
    v.toLowerCase();
    // "master"/"slave" de la API anterior: el master recibía, el slave solo enviaba
    if (v == "receptor" || v == "master") { out = ESPNOW_ROLE_RECEPTOR; return true; }
    if (v == "emisor" || v == "slave") { out = ESPNOW_ROLE_EMISOR; return true; }
    return false;
}

bool ESPNowConfig::setPairingKey(const String& key) {
    if (key.length() < KEY_MIN_LEN || key.length() > KEY_MAX_LEN) return false;
    pairingKey = key;
    return true;
}

bool ESPNowConfig::upsertPeer(const uint8_t* mac, const String& name, EspNowRole peerRole, bool& changed) {
    changed = false;
    String safeName = name.substring(0, 31);
    for (auto& p : peers) {
        if (memcmp(p.mac, mac, 6) == 0) {
            if (p.name != safeName || p.role != peerRole) {
                p.name = safeName;
                p.role = peerRole;
                changed = true;
            }
            return true;
        }
    }
    if (peers.size() >= MAX_PEERS) return false;
    EspNowPairedPeer p;
    memcpy(p.mac, mac, 6);
    p.name = safeName;
    p.role = peerRole;
    peers.push_back(p);
    changed = true;
    return true;
}

bool ESPNowConfig::removePeer(const uint8_t* mac) {
    for (auto it = peers.begin(); it != peers.end(); ++it) {
        if (memcmp(it->mac, mac, 6) == 0) {
            peers.erase(it);
            return true;
        }
    }
    return false;
}

uint8_t ESPNowConfig::getSensorID() const {
    if (useAutoID || customSensorID == 0) {
        return autoSensorID;
    }
    return customSensorID;
}

String ESPNowConfig::getSensorName() const {
    if (useAutoName || customSensorName.length() == 0) {
        return autoSensorName;
    }
    return customSensorName;
}

void ESPNowConfig::setCustomSensorID(uint8_t id) {
    if (id >= 1 && id <= 254) {
        customSensorID = id;
        useAutoID = false;
        Serial.printf("🆔 Sensor ID personalizado: %d\n", id);
    }
}

void ESPNowConfig::setCustomSensorName(const String& name) {
    if (name.length() > 0 && name.length() <= 31) {
        customSensorName = name;
        useAutoName = false;
        Serial.printf("🏷️ Nombre sensor personalizado: %s\n", name.c_str());
    }
}

void ESPNowConfig::toJson(JsonObject& json) const {
    // Device identity
    json["deviceMAC"] = deviceMAC;
    json["deviceChipID"] = deviceChipID;
    json["autoSensorID"] = autoSensorID;
    json["autoSensorName"] = autoSensorName;

    json["mode"] = modeToString(mode);
    json["role"] = roleToString(role);
    json["pairingKey"] = pairingKey;
    json["lastChannel"] = lastChannel;
    JsonArray arr = json.createNestedArray("peers");
    for (const auto& p : peers) {
        JsonObject o = arr.createNestedObject();
        o["mac"] = macToStr(p.mac);
        o["name"] = p.name;
        o["role"] = roleToString(p.role);
    }

    // Custom identity
    json["customSensorID"] = customSensorID;
    json["customSensorName"] = customSensorName;
    json["useAutoID"] = useAutoID;
    json["useAutoName"] = useAutoName;
}

void ESPNowConfig::fromJson(const JsonObject& json) {
    // Configs anteriores a 3.5.0 no tienen "mode": quedan en Automático. Los antiguos
    // isMasterMode/isSlaveMode se autodetectaban (todas las unidades con WiFi eran
    // master), así que no se migran: el rol arranca como emisor
    EspNowMode m;
    mode = modeFromString(json["mode"] | "automatico", m) ? m : ESPNOW_MODE_AUTO;
    EspNowRole r;
    role = roleFromString(json["role"] | "emisor", r) ? r : ESPNOW_ROLE_EMISOR;
    pairingKey = json["pairingKey"] | "";
    if (pairingKey.length() > KEY_MAX_LEN) pairingKey = "";
    lastChannel = json["lastChannel"] | 0;
    if (lastChannel > 13) lastChannel = 0;
    peers.clear();
    JsonArrayConst arr = json["peers"];
    for (JsonObjectConst o : arr) {
        if (peers.size() >= MAX_PEERS) break;
        EspNowPairedPeer p;
        if (!strToMac(o["mac"] | "", p.mac)) continue;
        p.name = o["name"] | "";
        EspNowRole pr;
        p.role = roleFromString(o["role"] | "emisor", pr) ? pr : ESPNOW_ROLE_EMISOR;
        peers.push_back(p);
    }

    // Custom identity
    customSensorID = json["customSensorID"] | 0;
    customSensorName = json["customSensorName"] | "";
    useAutoID = json["useAutoID"] | true;
    useAutoName = json["useAutoName"] | true;

    // Re-generate auto identity if needed
    if (deviceMAC.length() == 0) {
        generateAutoIdentity();
    }
}
