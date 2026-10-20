#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

// Estructura para una red WiFi individual
struct WiFiNetwork {
    String ssid;
    String password;
    bool is_default;
    
    WiFiNetwork() : is_default(false) {}
    WiFiNetwork(const String& s, const String& p, bool def = false) 
        : ssid(s), password(p), is_default(def) {}
};

struct NetworkSettings {
    String hostname;
    String ap_ssid;      // SoftAP SSID
    String ap_password;  // SoftAP Password
    std::vector<WiFiNetwork> wifi_networks; // Lista de redes WiFi
    bool check_updates;  // OTA update check enabled
    
    // Métodos de compatibilidad con código existente
    String wifi_ssid;    // DEPRECATED: Para compatibilidad
    String wifi_password; // DEPRECATED: Para compatibilidad
};

class NetworkConfig {
public:
    NetworkConfig();
    
    // Core functionality
    bool loadFromJson(const JsonObject& json);
    void saveToJson(JsonObject& json) const;
    void setDefaults();
    
    // Hostname management
    void setHostname(const String& hostname);
    String getHostname() const { return settings.hostname; }
    
    // SoftAP configuration
    void setApSSID(const String& ssid);
    String getApSSID() const { return settings.ap_ssid; }
    
    void setApPassword(const String& password);
    String getApPassword() const { return settings.ap_password; }
    
    // Station WiFi credentials management (LEGACY - mantener compatibilidad)
    bool setWiFiCredentials(const String& ssid, const String& password);
    String getWiFiSSID() const { return settings.wifi_ssid; }
    String getWiFiPassword() const { return settings.wifi_password; }
    
    // Multi-WiFi Network Management (NEW)
    bool addWiFiNetwork(const String& ssid, const String& password, bool is_default = false);
    bool removeWiFiNetwork(const String& ssid);
    bool setDefaultWiFiNetwork(const String& ssid);
    const std::vector<WiFiNetwork>& getWiFiNetworks() const { return settings.wifi_networks; }
    WiFiNetwork* getDefaultWiFiNetwork();
    WiFiNetwork* findWiFiNetwork(const String& ssid);
    void clearWiFiNetworks() { settings.wifi_networks.clear(); }
    int getWiFiNetworkCount() const { return settings.wifi_networks.size(); }
    
    // Update management
    void setCheckUpdates(bool enabled);
    bool getCheckUpdates() const { return settings.check_updates; }
    
    // Access to settings
    const NetworkSettings& getSettings() const { return settings; }
    
    // Validation
    bool isValid() const;
    
private:
    NetworkSettings settings;
    
    // Internal methods
    void ensureOneDefaultNetwork();
    
    // Default values
    static const char* DEFAULT_HOSTNAME;
    static const char* DEFAULT_AP_SSID;
    static const char* DEFAULT_AP_PASSWORD;
    static const bool DEFAULT_CHECK_UPDATES = true;
};

#endif // NETWORK_CONFIG_H