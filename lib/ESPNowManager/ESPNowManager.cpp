#include "ESPNowManager.h"
#include "UnifiedLogger.h"
#include <esp_wifi.h>
#include <Preferences.h>
#include <mbedtls/md.h>
#include <algorithm>

// Definición de variables estáticas
ESPNowManager* ESPNowManager::instance = nullptr;

static const size_t MAX_PEERS = 4;  // Igual que ESPNowConfig::MAX_PEERS
static const uint32_t HELLO_INTERVAL_MS = 3000;
static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const char KEY_CONTEXT[] = "TinacoESPNOW/v1|";

namespace {
// Mutex recursivo: el loop principal y los handlers web (tarea AsyncTCP) comparten la tabla
struct Lock {
    SemaphoreHandle_t m;
    explicit Lock(SemaphoreHandle_t mm) : m(mm) { xSemaphoreTakeRecursive(m, portMAX_DELAY); }
    ~Lock() { xSemaphoreGiveRecursive(m); }
};

bool tagEqual(const uint8_t* a, const uint8_t* b) {
    uint8_t diff = 0;
    for (int i = 0; i < ESPNOW_TAG_LEN; i++) diff |= a[i] ^ b[i];
    return diff == 0;
}

void deriveKey(const String& key, const char* suffix, uint8_t* out) {
    String input = String(KEY_CONTEXT) + key + suffix;
    mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
               (const unsigned char*)input.c_str(), input.length(), out);
}
}

// Singleton
ESPNowManager* ESPNowManager::getInstance() {
    if (instance == nullptr) {
        instance = new ESPNowManager();
    }
    return instance;
}

ESPNowManager::ESPNowManager() {
    mtx = xSemaphoreCreateRecursiveMutex();
    rxQueue = xQueueCreate(8, sizeof(RxItem));
    memset(hmacKey, 0, sizeof(hmacKey));
}

// Inicialización: sin clave no hay ESP-NOW (no se podría autenticar nada)
bool ESPNowManager::initESPNow() {
    Lock l(mtx);
    if (isInitialized) return true;
    if (!keySet) {
        UnifiedLogger::getInstance().warning("ESP-NOW sin clave de emparejamiento: no se inicia", "ESP-NOW");
        return false;
    }

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

    // Época de arranque en NVS (sobrevive a OTA y a borrar /config.json): las secuencias
    // de este arranque son mayores que las de cualquier arranque anterior
    if (!epochLoaded) {
        Preferences prefs;
        if (prefs.begin("espnow", false)) {
            bootEpoch = prefs.getUShort("epoch", 0) + 1;
            prefs.putUShort("epoch", bootEpoch);
            prefs.end();
        } else {
            UnifiedLogger::getInstance().error("NVS no disponible para la época ESP-NOW", "ESP-NOW");
        }
        seqCounter = 0;
        epochLoaded = true;
    }

    registerEspNowPeer(BROADCAST_MAC);
    for (const auto& p : peers) registerEspNowPeer(p.mac);

    UnifiedLogger::getInstance().info("ESP-NOW iniciado (canal " + String(WiFi.channel()) +
                                      ", " + String(peers.size()) + " unidades emparejadas)", "ESP-NOW");
    return true;
}

void ESPNowManager::deinitESPNow() {
    Lock l(mtx);
    if (!isInitialized) return;
    pairingOpen = false;
    esp_now_unregister_recv_cb();
    esp_now_unregister_send_cb();
    esp_now_deinit();
    isInitialized = false;
    xQueueReset(rxQueue);
    UnifiedLogger::getInstance().info("ESP-NOW apagado", "ESP-NOW");
}

void ESPNowManager::registerEspNowPeer(const uint8_t* mac) {
    if (!isInitialized || esp_now_is_peer_exist(mac)) return;
    esp_now_peer_info_t info = {};
    memcpy(info.peer_addr, mac, 6);
    info.channel = 0;          // Canal actual: el del router, o el último conocido sin WiFi
    info.ifidx = WIFI_IF_STA;
    info.encrypt = false;      // Autenticación por HMAC en la trama (abajo)
    esp_now_add_peer(&info);
}

// ===== Configuración =====

void ESPNowManager::setKey(const String& key) {
    Lock l(mtx);
    if (key.length() == 0) {
        keySet = false;
        memset(hmacKey, 0, sizeof(hmacKey));
        return;
    }
    deriveKey(key, "", hmacKey);
    keySet = true;
}

void ESPNowManager::setIdentity(uint8_t sensorId, const String& name, uint8_t role) {
    Lock l(mtx);
    localSensorId = sensorId;
    strncpy(localName, name.c_str(), sizeof(localName) - 1);
    localName[sizeof(localName) - 1] = '\0';
    localRole = role;
}

String ESPNowManager::getLocalName() {
    Lock l(mtx);
    return String(localName);
}

void ESPNowManager::setPairedPeers(const std::vector<PeerInfo>& list) {
    Lock l(mtx);
    std::vector<PeerState> old = peers;
    peers.clear();
    for (const auto& info : list) {
        if (peers.size() >= MAX_PEERS) break;
        PeerState p = {};
        memcpy(p.mac, info.mac, 6);
        strncpy(p.name, info.name.c_str(), sizeof(p.name) - 1);
        p.role = info.role;
        // Conservar la última secuencia vista (protección contra repetición)
        for (const auto& o : old) {
            if (memcmp(o.mac, p.mac, 6) == 0) {
                p.seqValid = o.seqValid;
                p.lastSeq = o.lastSeq;
                p.lastSeen = o.lastSeen;
                p.rxCount = o.rxCount;
            }
        }
        peers.push_back(p);
        registerEspNowPeer(p.mac);
    }
}

// ===== Emparejamiento =====

void ESPNowManager::startPairing(uint32_t durationMs) {
    Lock l(mtx);
    pairingStart = millis();
    pairingDuration = durationMs;
    lastHello = millis() - HELLO_INTERVAL_MS;  // Primer HELLO en el siguiente loop()
    pairingOpen = true;
    UnifiedLogger::getInstance().info("Emparejamiento abierto " + String(durationMs / 1000) + " s", "ESP-NOW");
}

void ESPNowManager::stopPairing() {
    Lock l(mtx);
    if (!pairingOpen) return;
    pairingOpen = false;
    UnifiedLogger::getInstance().info("Emparejamiento cerrado (" + String(peers.size()) + " unidades)", "ESP-NOW");
}

uint32_t ESPNowManager::pairingRemainingMs() const {
    if (!pairingOpen) return 0;
    uint32_t elapsed = millis() - pairingStart;
    return elapsed >= pairingDuration ? 0 : pairingDuration - elapsed;
}

void ESPNowManager::loop() {
    RxItem item;
    for (int n = 0; n < 8 && xQueueReceive(rxQueue, &item, 0) == pdTRUE; n++) {
        processFrame(item);
    }
    if (pairingOpen && isInitialized) {
        if (millis() - pairingStart >= pairingDuration) {
            stopPairing();
        } else if (millis() - lastHello >= HELLO_INTERVAL_MS) {
            broadcastHello();
            lastHello = millis();
        }
    }
}

// ===== Recepción =====

// Contexto de la tarea WiFi: solo descartar lo que no es nuestro protocolo y encolar.
// Autenticación y despacho van en loop()
void ESPNowManager::onDataReceived(const uint8_t *mac, const uint8_t *data, int len) {
    ESPNowManager* mgr = instance;
    if (!mgr || !mgr->rxQueue) return;
    if (len < (int)(sizeof(EspNowWireHeader) + ESPNOW_TAG_LEN) || len > ESPNOW_MAX_FRAME ||
        data[0] != ESPNOW_PROTO_MAGIC0 || data[1] != ESPNOW_PROTO_MAGIC1 ||
        data[2] != ESPNOW_PROTO_VERSION) {
        mgr->stats.rxMalformed++;
        return;
    }
    RxItem item;
    memcpy(item.mac, mac, 6);
    item.len = (uint8_t)len;
    memcpy(item.data, data, len);
    if (xQueueSend(mgr->rxQueue, &item, 0) != pdTRUE) {
        mgr->stats.rxDropped++;
    }
}

void ESPNowManager::onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    ESPNowManager* mgr = instance;
    if (!mgr) return;
    if (status == ESP_NOW_SEND_SUCCESS) mgr->stats.txOk++;
    else mgr->stats.txFail++;
}

void ESPNowManager::processFrame(const RxItem& item) {
    const size_t hdrLen = sizeof(EspNowWireHeader);
    EspNowWireHeader h;
    memcpy(&h, item.data, hdrLen);
    const uint8_t* payload = item.data + hdrLen;
    const size_t payloadLen = item.len - hdrLen - ESPNOW_TAG_LEN;
    const bool isHello = (h.type == PKT_HELLO || h.type == PKT_HELLO_ACK);

    // Acciones fuera del mutex (los callbacks pueden volver a llamar al manager)
    bool firePeer = false, fireSensor = false, replyAck = false, replyPong = false;
    char peerName[32] = {0};
    uint8_t peerRole = ESPNOW_PEER_ROLE_EMISOR;
    SensorData sd = {};

    {
        Lock l(mtx);
        if (!keySet) return;

        // 1) Solo MACs emparejadas; una MAC nueva únicamente con HELLO y la ventana abierta
        PeerState* peer = findPeer(item.mac);
        if (!peer && !(isHello && pairingOpen)) {
            stats.rxUnknown++;
            return;
        }

        // 2) HMAC con la clave de emparejamiento
        uint8_t tag[ESPNOW_TAG_LEN];
        computeTag(hmacKey, item.mac, item.data, item.len - ESPNOW_TAG_LEN, tag);
        if (!tagEqual(tag, item.data + item.len - ESPNOW_TAG_LEN)) {
            stats.rxBadAuth++;
            return;
        }

        // 3) Repetición: la secuencia de cada unidad solo crece
        if (peer && peer->seqValid && h.seq <= peer->lastSeq) {
            stats.rxReplay++;
            return;
        }

        EspNowHelloPayload hello = {};
        if (isHello) {
            if (payloadLen < sizeof(EspNowHelloPayload)) {
                stats.rxMalformed++;
                return;
            }
            memcpy(&hello, payload, sizeof(hello));
            hello.name[sizeof(hello.name) - 1] = '\0';
        }

        if (!peer) {
            if (!addPeerLocked(item.mac, hello.name, hello.role)) {
                UnifiedLogger::getInstance().warning("Tabla de unidades llena: " + macToString(item.mac), "ESP-NOW");
                return;
            }
            peer = findPeer(item.mac);
            UnifiedLogger::getInstance().info("Unidad emparejada: " + macToString(item.mac) + " (" + String(hello.name) + ")", "ESP-NOW");
            firePeer = true;
        }

        peer->seqValid = true;
        peer->lastSeq = h.seq;
        peer->lastSeen = millis();
        peer->rxCount++;
        stats.rxOk++;

        switch (h.type) {
            case PKT_HELLO:
            case PKT_HELLO_ACK:
                if (strncmp(peer->name, hello.name, sizeof(peer->name)) != 0 || peer->role != hello.role) {
                    strncpy(peer->name, hello.name, sizeof(peer->name) - 1);
                    peer->role = hello.role;
                    firePeer = true;
                }
                replyAck = (h.type == PKT_HELLO);
                break;
            case PKT_SENSOR: {
                if (payloadLen < sizeof(EspNowSensorPayload)) {
                    stats.rxMalformed++;
                    break;
                }
                EspNowSensorPayload sp;
                memcpy(&sp, payload, sizeof(sp));
                memcpy(sd.mac, item.mac, 6);
                sd.sensorId = sp.sensorId;
                sd.flags = sp.flags;
                sd.liquidLevel = sp.percent;
                sd.distance = sp.distance;
                sd.liters = sp.liters;
                sd.rssi = sp.rssi;
                sd.timestamp = millis();
                memcpy(sd.sensorName, sp.name, sizeof(sd.sensorName));
                sd.sensorName[sizeof(sd.sensorName) - 1] = '\0';
                bool found = false;
                for (auto& s : connectedSensors) {
                    if (memcmp(s.mac, sd.mac, 6) == 0) { s = sd; found = true; break; }
                }
                if (!found) connectedSensors.push_back(sd);
                fireSensor = true;
                break;
            }
            case PKT_PING:
                replyPong = true;
                break;
            default:
                break;
        }
        memcpy(peerName, peer->name, sizeof(peerName));
        peerRole = peer->role;
    }

    if (replyAck) sendHelloTo(item.mac, PKT_HELLO_ACK);
    if (replyPong) sendFrame(item.mac, PKT_PONG, nullptr, 0);
    if (firePeer && onPeerUpdated) onPeerUpdated(item.mac, String(peerName), peerRole);
    if (fireSensor && onSensorDataCallback) onSensorDataCallback(sd);
}

// ===== Envío =====

void ESPNowManager::computeTag(const uint8_t* key, const uint8_t* srcMac, const uint8_t* frame, size_t len, uint8_t* tagOut) {
    uint8_t buf[6 + ESPNOW_MAX_FRAME];
    memcpy(buf, srcMac, 6);
    memcpy(buf + 6, frame, len);
    uint8_t full[32];
    mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), key, 32, buf, 6 + len, full);
    memcpy(tagOut, full, ESPNOW_TAG_LEN);
}

uint32_t ESPNowManager::nextSeq() {
    if (seqCounter == 0xFFFF) {
        // ~65k tramas en un arranque: nueva época para seguir creciendo
        bootEpoch++;
        seqCounter = 0;
        Preferences prefs;
        if (prefs.begin("espnow", false)) {
            prefs.putUShort("epoch", bootEpoch);
            prefs.end();
        }
    }
    seqCounter++;
    return ((uint32_t)bootEpoch << 16) | seqCounter;
}

static bool buildFrame(uint8_t* frame, size_t& total, uint8_t type, uint32_t seq, const void* payload, size_t len) {
    const size_t hdrLen = sizeof(EspNowWireHeader);
    total = hdrLen + len + ESPNOW_TAG_LEN;
    if (total > ESPNOW_MAX_FRAME) return false;
    EspNowWireHeader h;
    h.magic[0] = ESPNOW_PROTO_MAGIC0;
    h.magic[1] = ESPNOW_PROTO_MAGIC1;
    h.version = ESPNOW_PROTO_VERSION;
    h.type = type;
    h.seq = seq;
    memcpy(frame, &h, hdrLen);
    if (len) memcpy(frame + hdrLen, payload, len);
    return true;
}

bool ESPNowManager::sendFrame(const uint8_t* dst, uint8_t type, const void* payload, size_t len) {
    Lock l(mtx);
    if (!isInitialized || !keySet) return false;
    uint8_t frame[ESPNOW_MAX_FRAME];
    size_t total;
    if (!buildFrame(frame, total, type, nextSeq(), payload, len)) return false;
    uint8_t self[6];
    esp_wifi_get_mac(WIFI_IF_STA, self);
    computeTag(hmacKey, self, frame, total - ESPNOW_TAG_LEN, frame + total - ESPNOW_TAG_LEN);
    if (esp_now_send(dst, frame, total) != ESP_OK) {
        stats.txFail++;
        return false;
    }
    if (memcmp(dst, BROADCAST_MAC, 6) != 0) {
        memcpy(lastFrame, frame, total);
        lastFrameLen = (uint8_t)total;
        memcpy(lastFrameDst, dst, 6);
    }
    return true;
}

void ESPNowManager::sendHelloTo(const uint8_t* mac, uint8_t type) {
    EspNowHelloPayload hello = {};
    {
        Lock l(mtx);
        hello.role = localRole;
        hello.sensorId = localSensorId;
        memcpy(hello.name, localName, sizeof(hello.name));
    }
    sendFrame(mac, type, &hello, sizeof(hello));
}

void ESPNowManager::broadcastHello() {
    sendHelloTo(BROADCAST_MAC, PKT_HELLO);
}

bool ESPNowManager::sendSensorToPeers(const EspNowSensorPayload& data) {
    Lock l(mtx);
    bool any = false;
    for (const auto& p : peers) {
        if (sendFrame(p.mac, PKT_SENSOR, &data, sizeof(data))) any = true;
    }
    return any;
}

bool ESPNowManager::sendPing(const uint8_t* mac) {
    return sendFrame(mac, PKT_PING, nullptr, 0);
}

bool ESPNowManager::pingAll() {
    Lock l(mtx);
    bool any = false;
    for (const auto& p : peers) {
        if (sendPing(p.mac)) any = true;
    }
    return any;
}

// Reenvía tal cual la última trama unicast: el receptor debe contarla en rxReplay
bool ESPNowManager::testReplayLast() {
    Lock l(mtx);
    if (!isInitialized || lastFrameLen == 0) return false;
    return esp_now_send(lastFrameDst, lastFrame, lastFrameLen) == ESP_OK;
}

// Trama SENSOR firmada con otra clave: el receptor debe contarla en rxBadAuth
bool ESPNowManager::testBadKey() {
    Lock l(mtx);
    if (!isInitialized || !keySet || peers.empty()) return false;
    uint8_t wrongKey[32];
    deriveKey(String((const char*)localName), "/clave-equivocada", wrongKey);
    EspNowSensorPayload fake = {};
    fake.sensorId = localSensorId;
    fake.percent = 100.0f;
    strncpy(fake.name, "FALSO", sizeof(fake.name) - 1);
    uint8_t self[6];
    esp_wifi_get_mac(WIFI_IF_STA, self);
    bool any = false;
    for (const auto& p : peers) {
        uint8_t frame[ESPNOW_MAX_FRAME];
        size_t total;
        if (!buildFrame(frame, total, PKT_SENSOR, nextSeq(), &fake, sizeof(fake))) return false;
        computeTag(wrongKey, self, frame, total - ESPNOW_TAG_LEN, frame + total - ESPNOW_TAG_LEN);
        if (esp_now_send(p.mac, frame, total) == ESP_OK) any = true;
    }
    return any;
}

// ===== Gestión de peers =====

ESPNowManager::PeerState* ESPNowManager::findPeer(const uint8_t* mac) {
    for (auto& p : peers) {
        if (memcmp(p.mac, mac, 6) == 0) return &p;
    }
    return nullptr;
}

bool ESPNowManager::addPeerLocked(const uint8_t* mac, const char* name, uint8_t role) {
    if (findPeer(mac)) return true;
    if (peers.size() >= MAX_PEERS) return false;
    PeerState p = {};
    memcpy(p.mac, mac, 6);
    strncpy(p.name, name ? name : "", sizeof(p.name) - 1);
    p.role = role;
    peers.push_back(p);
    registerEspNowPeer(mac);
    return true;
}

bool ESPNowManager::addPeerManual(const uint8_t* mac) {
    Lock l(mtx);
    if (memcmp(mac, BROADCAST_MAC, 6) == 0) return false;
    return addPeerLocked(mac, "", ESPNOW_PEER_ROLE_EMISOR);
}

bool ESPNowManager::removePeer(const uint8_t* macAddress) {
    Lock l(mtx);
    bool removed = false;
    for (auto it = peers.begin(); it != peers.end(); ++it) {
        if (memcmp(it->mac, macAddress, 6) == 0) {
            peers.erase(it);
            removed = true;
            break;
        }
    }
    if (isInitialized && esp_now_is_peer_exist(macAddress)) esp_now_del_peer(macAddress);
    connectedSensors.erase(std::remove_if(connectedSensors.begin(), connectedSensors.end(),
        [&](const SensorData& s) { return memcmp(s.mac, macAddress, 6) == 0; }), connectedSensors.end());
    if (removed) UnifiedLogger::getInstance().info("Unidad eliminada: " + macToString(macAddress), "ESP-NOW");
    return removed;
}

bool ESPNowManager::hasPeers() {
    Lock l(mtx);
    return !peers.empty();
}

size_t ESPNowManager::getPeerCount() {
    Lock l(mtx);
    return peers.size();
}

// ===== Lecturas remotas =====

std::vector<SensorData> ESPNowManager::getConnectedSensors() {
    Lock l(mtx);
    return connectedSensors;
}

uint8_t ESPNowManager::getConnectedSensorCount() {
    Lock l(mtx);
    return connectedSensors.size();
}

void ESPNowManager::clearOldSensors(uint32_t maxAge) {
    Lock l(mtx);
    uint32_t now = millis();
    connectedSensors.erase(
        std::remove_if(connectedSensors.begin(), connectedSensors.end(),
            [now, maxAge](const SensorData& s) { return (now - s.timestamp) > maxAge; }),
        connectedSensors.end()
    );
}

// ===== JSON para la API =====

void ESPNowManager::peersToJson(JsonArray arr) {
    Lock l(mtx);
    uint32_t now = millis();
    for (const auto& p : peers) {
        JsonObject o = arr.createNestedObject();
        o["mac"] = macToString(p.mac);
        o["name"] = p.name;
        o["role"] = p.role == ESPNOW_PEER_ROLE_RECEPTOR ? "receptor" : "emisor";
        o["last_seen_s"] = p.lastSeen ? (long)((now - p.lastSeen) / 1000) : -1;
        o["rx"] = p.rxCount;
    }
}

void ESPNowManager::sensorsToJson(JsonArray arr) {
    Lock l(mtx);
    uint32_t now = millis();
    for (const auto& s : connectedSensors) {
        JsonObject o = arr.createNestedObject();
        o["mac"] = macToString(s.mac);
        o["id"] = s.sensorId;
        o["name"] = s.sensorName;
        o["valid"] = (s.flags & ESPNOW_FLAG_VALID) != 0;
        o["percent"] = serialized(String(s.liquidLevel, 1));
        o["liters"] = serialized(String(s.liters, 1));
        o["distance"] = serialized(String(s.distance, 1));
        o["rssi"] = s.rssi;
        o["age_s"] = (now - s.timestamp) / 1000;
    }
}

void ESPNowManager::statsToJson(JsonObject obj) {
    obj["rx_ok"] = stats.rxOk;
    obj["rx_bad_auth"] = stats.rxBadAuth;
    obj["rx_replay"] = stats.rxReplay;
    obj["rx_unknown"] = stats.rxUnknown;
    obj["rx_malformed"] = stats.rxMalformed;
    obj["rx_dropped"] = stats.rxDropped;
    obj["tx_ok"] = stats.txOk;
    obj["tx_fail"] = stats.txFail;
}

String ESPNowManager::macToString(const uint8_t* mac) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}

bool ESPNowManager::stringToMac(const String& macStr, uint8_t* mac) {
    if (macStr.length() < 17) return false;
    return sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) == 6;
}
