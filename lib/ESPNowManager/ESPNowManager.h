#ifndef ESPNOW_MANAGER_H
#define ESPNOW_MANAGER_H

#include <esp_now.h>
#include <WiFi.h>
#include <vector>
#include <functional>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// ===== Protocolo autenticado (3.5.0) =====
// Trama: [cabecera 8 B][payload][etiqueta 12 B]
// etiqueta = HMAC-SHA256(SHA256("TinacoESPNOW/v1|" + clave), MAC origen + cabecera + payload),
// truncado a 12 bytes. Sin clave correcta, MAC emparejada y secuencia nueva, la trama se descarta.
// Nunca viajan credenciales WiFi ni MQTT: solo identidad, rol y lecturas.
#define ESPNOW_PROTO_MAGIC0  'T'
#define ESPNOW_PROTO_MAGIC1  'K'
#define ESPNOW_PROTO_VERSION 1
#define ESPNOW_TAG_LEN       12
#define ESPNOW_MAX_FRAME     250

enum EspNowPacketType : uint8_t {
    PKT_HELLO = 1,      // Broadcast durante el emparejamiento
    PKT_HELLO_ACK = 2,  // Respuesta unicast al HELLO
    PKT_SENSOR = 3,     // Lectura del tinaco local
    PKT_PING = 4,
    PKT_PONG = 5
};

// Roles: mismos valores que EspNowRole (lib/Config/Domain/ESPNowConfig.h)
#define ESPNOW_PEER_ROLE_EMISOR   0
#define ESPNOW_PEER_ROLE_RECEPTOR 1

#pragma pack(push, 1)
struct EspNowWireHeader {
    uint8_t magic[2];
    uint8_t version;
    uint8_t type;
    uint32_t seq;       // (época de arranque << 16) | contador: siempre creciente entre reinicios
};

struct EspNowHelloPayload {
    uint8_t role;
    uint8_t sensorId;
    char name[32];
};

#define ESPNOW_FLAG_VALID   0x01  // Lectura válida (sensor con eco)
#define ESPNOW_FLAG_FILLING 0x02
#define ESPNOW_FLAG_STABLE  0x04

struct EspNowSensorPayload {
    uint8_t sensorId;
    uint8_t flags;
    float percent;
    float distance;
    float liters;
    int8_t rssi;        // RSSI WiFi del emisor (0 sin WiFi)
    char name[32];
};
#pragma pack(pop)

// Lectura recibida de una unidad emparejada
struct SensorData {
    uint8_t mac[6];
    uint8_t sensorId;
    uint8_t flags;
    float liquidLevel;          // Porcentaje
    float distance;             // cm
    float liters;
    uint32_t timestamp;         // millis() local al recibirla
    char sensorName[32];
    int8_t rssi;
};

// Contadores para verificar el filtrado (GET /api/esp-now/status)
struct EspNowStats {
    uint32_t rxOk = 0;
    uint32_t rxBadAuth = 0;     // HMAC incorrecto (otra clave o trama alterada)
    uint32_t rxReplay = 0;      // Secuencia repetida o vieja
    uint32_t rxUnknown = 0;     // MAC no emparejada fuera de la ventana
    uint32_t rxMalformed = 0;   // Otro protocolo / tamaño incorrecto
    uint32_t rxDropped = 0;     // Cola llena
    uint32_t txOk = 0;
    uint32_t txFail = 0;
};

class ESPNowManager {
public:
    static ESPNowManager* getInstance();

    // Ciclo de vida
    bool initESPNow();
    void deinitESPNow();
    bool isInit() const { return isInitialized; }
    void loop();  // Procesa la cola de recepción y el emparejamiento (contexto del loop principal)

    // Configuración
    void setKey(const String& key);           // Vacía = sin clave (no envía ni acepta nada)
    bool hasKey() const { return keySet; }
    void setIdentity(uint8_t sensorId, const String& name, uint8_t role);
    String getLocalName();
    // Carga las unidades emparejadas guardadas (sustituye la tabla)
    struct PeerInfo {
        uint8_t mac[6];
        String name;
        uint8_t role;
    };
    void setPairedPeers(const std::vector<PeerInfo>& list);

    // Emparejamiento: solo en esta ventana se agregan unidades nuevas (con clave válida)
    void startPairing(uint32_t durationMs);
    void stopPairing();
    bool isPairing() const { return pairingOpen; }
    uint32_t pairingRemainingMs() const;

    // Unidades emparejadas
    bool addPeerManual(const uint8_t* mac);
    bool removePeer(const uint8_t* mac);
    bool hasPeers();
    size_t getPeerCount();

    // Envío (todas las tramas autenticadas)
    bool sendSensorToPeers(const EspNowSensorPayload& data);
    bool sendPing(const uint8_t* mac);
    bool pingAll();
    // Pruebas de seguridad: el receptor debe contarlas en rxReplay / rxBadAuth
    bool testReplayLast();
    bool testBadKey();

    // Lecturas remotas
    std::vector<SensorData> getConnectedSensors();
    uint8_t getConnectedSensorCount();
    void clearOldSensors(uint32_t maxAge = 600000);

    // Estado
    const EspNowStats& getStats() const { return stats; }
    uint32_t getPacketsSent() const { return stats.txOk + stats.txFail; }
    uint32_t getPacketsReceived() const { return stats.rxOk; }
    void peersToJson(JsonArray arr);
    void sensorsToJson(JsonArray arr);
    void statsToJson(JsonObject obj);

    String macToString(const uint8_t* mac);
    bool stringToMac(const String& macStr, uint8_t* mac);

    // Callbacks (se llaman desde loop(), nunca desde el callback de la radio)
    std::function<void(const uint8_t* mac, const String& name, uint8_t role)> onPeerUpdated;
    std::function<void(const SensorData&)> onSensorDataCallback;

private:
    ESPNowManager();
    static ESPNowManager* instance;

    struct PeerState {
        uint8_t mac[6];
        char name[32];
        uint8_t role;
        bool seqValid;
        uint32_t lastSeq;
        uint32_t lastSeen;   // millis() de la última trama válida (0 = nunca)
        uint32_t rxCount;
    };

    struct RxItem {
        uint8_t mac[6];
        uint8_t len;
        uint8_t data[ESPNOW_MAX_FRAME];
    };

    static void onDataReceived(const uint8_t *mac, const uint8_t *data, int len);
    static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);

    void processFrame(const RxItem& item);
    bool sendFrame(const uint8_t* dst, uint8_t type, const void* payload, size_t len);
    void computeTag(const uint8_t* key, const uint8_t* srcMac, const uint8_t* frame, size_t len, uint8_t* tagOut);
    uint32_t nextSeq();
    PeerState* findPeer(const uint8_t* mac);
    bool addPeerLocked(const uint8_t* mac, const char* name, uint8_t role);
    void registerEspNowPeer(const uint8_t* mac);
    void broadcastHello();
    void sendHelloTo(const uint8_t* mac, uint8_t type);

    std::vector<PeerState> peers;
    std::vector<SensorData> connectedSensors;

    SemaphoreHandle_t mtx = nullptr;   // Recursivo: tabla de peers, secuencia, clave
    QueueHandle_t rxQueue = nullptr;

    bool isInitialized = false;
    bool keySet = false;
    uint8_t hmacKey[32];

    uint8_t localSensorId = 0;
    char localName[32] = {0};
    uint8_t localRole = ESPNOW_PEER_ROLE_EMISOR;

    bool epochLoaded = false;
    uint16_t bootEpoch = 0;
    uint16_t seqCounter = 0;

    volatile bool pairingOpen = false;
    uint32_t pairingStart = 0;
    uint32_t pairingDuration = 0;
    uint32_t lastHello = 0;

    // Última trama enviada (solo para testReplayLast)
    uint8_t lastFrame[ESPNOW_MAX_FRAME];
    uint8_t lastFrameLen = 0;
    uint8_t lastFrameDst[6];

    EspNowStats stats;
};

#endif // ESPNOW_MANAGER_H
