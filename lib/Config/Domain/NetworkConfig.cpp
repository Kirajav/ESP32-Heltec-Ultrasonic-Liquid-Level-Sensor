#include "NetworkConfig.h"

// Default values
const char* NetworkConfig::DEFAULT_HOSTNAME = "sensor-nivel-liquido";
const char* NetworkConfig::DEFAULT_AP_SSID = "ESP32: Sensor de nivel de liquido";
const char* NetworkConfig::DEFAULT_AP_PASSWORD = "12345678";

NetworkConfig::NetworkConfig() {
    setDefaults();
}

bool NetworkConfig::loadFromJson(const JsonObject& json) {
    if (json.containsKey("hostname")) {
        settings.hostname = json["hostname"].as<String>();
    }
    
    if (json.containsKey("ap_ssid")) {
        settings.ap_ssid = json["ap_ssid"].as<String>();
    }
    
    if (json.containsKey("ap_password")) {
        settings.ap_password = json["ap_password"].as<String>();
    }
    
    if (json.containsKey("wifi_ssid")) {
        settings.wifi_ssid = json["wifi_ssid"].as<String>();
    }
    
    if (json.containsKey("wifi_password")) {
        settings.wifi_password = json["wifi_password"].as<String>();
    }
    
    // Load wifi networks array
    if (json.containsKey("wifi_networks")) {
        JsonArray networksArray = json["wifi_networks"];
        settings.wifi_networks.clear();
        
        for (JsonObject networkObj : networksArray) {
            WiFiNetwork network;
            network.ssid = networkObj["ssid"].as<String>();
            network.password = networkObj["password"].as<String>();
            network.is_default = networkObj["is_default"].as<bool>();
            
            if (!network.ssid.isEmpty()) {
                settings.wifi_networks.push_back(network);
            }
        }
        
        // If no networks loaded but legacy fields exist, migrate them
        if (settings.wifi_networks.empty() && !settings.wifi_ssid.isEmpty()) {
            WiFiNetwork legacyNetwork;
            legacyNetwork.ssid = settings.wifi_ssid;
            legacyNetwork.password = settings.wifi_password;
            legacyNetwork.is_default = true;
            settings.wifi_networks.push_back(legacyNetwork);
        }
        
        // Ensure exactly one default network
        ensureOneDefaultNetwork();
    } else {
        // Legacy mode: create network from wifi_ssid/wifi_password if they exist
        if (!settings.wifi_ssid.isEmpty()) {
            settings.wifi_networks.clear();
            WiFiNetwork legacyNetwork;
            legacyNetwork.ssid = settings.wifi_ssid;
            legacyNetwork.password = settings.wifi_password;
            legacyNetwork.is_default = true;
            settings.wifi_networks.push_back(legacyNetwork);
        }
    }
    
    if (json.containsKey("check_updates")) {
        settings.check_updates = json["check_updates"].as<bool>();
    }
    
    return isValid();
}

void NetworkConfig::saveToJson(JsonObject& json) const {
    json["hostname"] = settings.hostname;
    json["ap_ssid"] = settings.ap_ssid;
    json["ap_password"] = settings.ap_password;
    json["wifi_ssid"] = settings.wifi_ssid;
    json["wifi_password"] = settings.wifi_password;
    json["check_updates"] = settings.check_updates;
    
    // Save wifi networks array
    JsonArray networksArray = json.createNestedArray("wifi_networks");
    for (const WiFiNetwork& network : settings.wifi_networks) {
        JsonObject networkObj = networksArray.createNestedObject();
        networkObj["ssid"] = network.ssid;
        networkObj["password"] = network.password;
        networkObj["is_default"] = network.is_default;
    }
}

void NetworkConfig::setDefaults() {
    settings.hostname = DEFAULT_HOSTNAME;
    settings.ap_ssid = DEFAULT_AP_SSID;
    settings.ap_password = DEFAULT_AP_PASSWORD;
    settings.wifi_ssid = "";        // No WiFi por defecto
    settings.wifi_password = "";    // No password por defecto
    settings.wifi_networks.clear(); // Limpiar lista de redes WiFi
    settings.check_updates = true;  // DEFAULT_CHECK_UPDATES
}

void NetworkConfig::setHostname(const String& hostname) {
    if (hostname.length() > 0 && hostname.length() <= 32) {
        settings.hostname = hostname;
    }
}

void NetworkConfig::setApSSID(const String& ssid) {
    if (ssid.length() > 0 && ssid.length() <= 32) {
        settings.ap_ssid = ssid;
    }
}

void NetworkConfig::setApPassword(const String& password) {
    if (password.length() >= 8 && password.length() <= 63) {
        settings.ap_password = password;
    }
}

bool NetworkConfig::setWiFiCredentials(const String& ssid, const String& password) {
    // Validar SSID
    if (ssid.length() == 0 || ssid.length() > 32) {
        return false;
    }
    
    // Validar password (puede estar vacío para redes abiertas)
    if (password.length() > 63) { // WPA/WPA2 max length
        return false;
    }
    
    // Si llegamos aquí, las credenciales son válidas
    settings.wifi_ssid = ssid;
    settings.wifi_password = password;
    return true;
}

void NetworkConfig::setCheckUpdates(bool enabled) {
    settings.check_updates = enabled;
}

// ===== MULTI-WIFI NETWORK MANAGEMENT =====

bool NetworkConfig::addWiFiNetwork(const String& ssid, const String& password, bool is_default) {
    // Validar SSID
    if (ssid.length() == 0 || ssid.length() > 32) {
        return false;
    }
    
    // Validar password
    if (password.length() > 63) {
        return false;
    }
    
    // Verificar si ya existe
    for (auto& network : settings.wifi_networks) {
        if (network.ssid == ssid) {
            // Actualizar red existente
            network.password = password;
            network.is_default = is_default;
            
            // Si se marca como default, quitar default de otras
            if (is_default) {
                for (auto& other : settings.wifi_networks) {
                    if (other.ssid != ssid) {
                        other.is_default = false;
                    }
                }
            }
            
            // Actualizar campos legacy para compatibilidad
            if (is_default) {
                settings.wifi_ssid = ssid;
                settings.wifi_password = password;
            }
            
            return true;
        }
    }
    
    // Si se marca como default, quitar default de todas las existentes
    if (is_default) {
        for (auto& network : settings.wifi_networks) {
            network.is_default = false;
        }
        
        // Actualizar campos legacy
        settings.wifi_ssid = ssid;
        settings.wifi_password = password;
    }
    
    // Agregar nueva red
    settings.wifi_networks.emplace_back(ssid, password, is_default);
    return true;
}

bool NetworkConfig::removeWiFiNetwork(const String& ssid) {
    for (auto it = settings.wifi_networks.begin(); it != settings.wifi_networks.end(); ++it) {
        if (it->ssid == ssid) {
            bool was_default = it->is_default;
            settings.wifi_networks.erase(it);
            
            // Si era la red default, limpiar campos legacy
            if (was_default) {
                settings.wifi_ssid = "";
                settings.wifi_password = "";
                
                // Si hay otras redes, hacer la primera como default
                if (!settings.wifi_networks.empty()) {
                    settings.wifi_networks[0].is_default = true;
                    settings.wifi_ssid = settings.wifi_networks[0].ssid;
                    settings.wifi_password = settings.wifi_networks[0].password;
                }
            }
            
            return true;
        }
    }
    return false;
}

bool NetworkConfig::setDefaultWiFiNetwork(const String& ssid) {
    bool found = false;
    
    for (auto& network : settings.wifi_networks) {
        if (network.ssid == ssid) {
            network.is_default = true;
            found = true;
            
            // Actualizar campos legacy
            settings.wifi_ssid = ssid;
            settings.wifi_password = network.password;
        } else {
            network.is_default = false;
        }
    }
    
    return found;
}

WiFiNetwork* NetworkConfig::getDefaultWiFiNetwork() {
    for (auto& network : settings.wifi_networks) {
        if (network.is_default) {
            return &network;
        }
    }
    
    // Si no hay default pero hay redes, devolver la primera
    if (!settings.wifi_networks.empty()) {
        return &settings.wifi_networks[0];
    }
    
    return nullptr;
}

WiFiNetwork* NetworkConfig::findWiFiNetwork(const String& ssid) {
    for (auto& network : settings.wifi_networks) {
        if (network.ssid == ssid) {
            return &network;
        }
    }
    return nullptr;
}

bool NetworkConfig::isValid() const {
    return settings.hostname.length() > 0 &&
           settings.hostname.length() <= 32 &&
           settings.ap_ssid.length() > 0 &&
           settings.ap_ssid.length() <= 32 &&
           settings.ap_password.length() >= 8 &&
           settings.ap_password.length() <= 63;
}

void NetworkConfig::ensureOneDefaultNetwork() {
    // Contar redes default
    size_t defaultCount = 0;
    int firstDefaultIndex = -1;
    
    for (size_t i = 0; i < settings.wifi_networks.size(); i++) {
        if (settings.wifi_networks[i].is_default) {
            if (firstDefaultIndex == -1) {
                firstDefaultIndex = i;
            } else {
                // Múltiples defaults encontrados, quitar este
                settings.wifi_networks[i].is_default = false;
            }
            defaultCount++;
        }
    }
    
    // Si no hay default pero hay redes, hacer la primera como default
    if (defaultCount == 0 && !settings.wifi_networks.empty()) {
        settings.wifi_networks[0].is_default = true;
    }
    
    // Actualizar campos legacy con la red default
    WiFiNetwork* defaultNetwork = getDefaultWiFiNetwork();
    if (defaultNetwork) {
        settings.wifi_ssid = defaultNetwork->ssid;
        settings.wifi_password = defaultNetwork->password;
    } else {
        settings.wifi_ssid = "";
        settings.wifi_password = "";
    }
}