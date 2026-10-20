#include "MQTTConfig.h"

MQTTConfig::MQTTConfig() {
    brokerHost = "";
    brokerPort = 1883;
    username = "";
    password = "";
    deviceName = "Sensor Nivel Agua";
    enabled = false;
}

bool MQTTConfig::isConfigured() const {
    return brokerHost.length() > 0;
}

bool MQTTConfig::isValid() const {
    return isConfigured() && brokerPort > 0 && brokerPort < 65536;
}

void MQTTConfig::toJson(JsonObject& json) const {
    json["brokerHost"] = brokerHost;
    json["brokerPort"] = brokerPort;
    json["username"] = username;
    json["password"] = password;
    json["deviceName"] = deviceName;
    json["enabled"] = enabled;
}

void MQTTConfig::fromJson(const JsonObject& json) {
    if (json.containsKey("brokerHost"))  brokerHost = json["brokerHost"].as<String>();
    if (json.containsKey("brokerPort"))  brokerPort = json["brokerPort"] | 1883;
    if (json.containsKey("username"))    username = json["username"].as<String>();
    if (json.containsKey("password"))    password = json["password"].as<String>();
    if (json.containsKey("deviceName"))  deviceName = json["deviceName"].as<String>();
    if (json.containsKey("enabled"))     enabled = json["enabled"] | false;
}
