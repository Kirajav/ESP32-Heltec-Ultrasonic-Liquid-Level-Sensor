#include "UnifiedLogger.h"
#include <WiFi.h>

// Global logger instance
UnifiedLogger& Logger = UnifiedLogger::getInstance();

UnifiedLogger::UnifiedLogger() :
    current_log_level(LogLevel::INFO),
    default_component("SYSTEM"),
    timestamps_enabled(true),
    web_output_enabled(true),
    serial_output_enabled(true),
    is_enabled(true)
{
}

bool UnifiedLogger::begin() {
    if (serial_output_enabled && !Serial) {
        Serial.begin(115200);
        delay(100);
    }
    
    info("Unified Logger initialized", "LOGGER");
    info("Log level: " + logLevelToString(current_log_level), "LOGGER");
    
    return true;
}

void UnifiedLogger::setLogLevel(LogLevel level) {
    current_log_level = level;
    info("Log level changed to: " + logLevelToString(level), "LOGGER");
}

LogLevel UnifiedLogger::getLogLevel() const {
    return current_log_level;
}

void UnifiedLogger::debug(const String& message, const String& component) {
    if (!is_enabled || current_log_level > LogLevel::DEBUG) return;
    
    String comp = component.isEmpty() ? default_component : component;
    String formatted = formatMessage(LogLevel::DEBUG, comp, message);
    outputMessage(formatted);
}

void UnifiedLogger::info(const String& message, const String& component) {
    if (!is_enabled || current_log_level > LogLevel::INFO) return;
    
    String comp = component.isEmpty() ? default_component : component;
    String formatted = formatMessage(LogLevel::INFO, comp, message);
    outputMessage(formatted);
}

void UnifiedLogger::warning(const String& message, const String& component) {
    if (!is_enabled || current_log_level > LogLevel::WARNING) return;
    
    String comp = component.isEmpty() ? default_component : component;
    String formatted = formatMessage(LogLevel::WARNING, comp, message);
    outputMessage(formatted);
}

void UnifiedLogger::error(const String& message, const String& component) {
    if (!is_enabled || current_log_level > LogLevel::ERROR) return;
    
    String comp = component.isEmpty() ? default_component : component;
    String formatted = formatMessage(LogLevel::ERROR, comp, message);
    outputMessage(formatted);
}

void UnifiedLogger::critical(const String& message, const String& component) {
    if (!is_enabled || current_log_level > LogLevel::CRITICAL) return;
    
    String comp = component.isEmpty() ? default_component : component;
    String formatted = formatMessage(LogLevel::CRITICAL, comp, message);
    outputMessage(formatted);
}

void UnifiedLogger::logf(LogLevel level, const String& component, const char* format, ...) {
    if (!is_enabled || current_log_level > level) return;
    
    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    String comp = component.isEmpty() ? default_component : component;
    String formatted = formatMessage(level, comp, String(buffer));
    outputMessage(formatted);
}

void UnifiedLogger::setComponent(const String& component) {
    default_component = component;
}

void UnifiedLogger::enableTimestamps(bool enable) {
    timestamps_enabled = enable;
}

void UnifiedLogger::enableWebOutput(bool enable) {
    web_output_enabled = enable;
}

void UnifiedLogger::enableSerialOutput(bool enable) {
    serial_output_enabled = enable;
}

bool UnifiedLogger::isEnabled() const {
    return is_enabled;
}

void UnifiedLogger::setEnabled(bool enabled) {
    is_enabled = enabled;
}

String UnifiedLogger::formatMessage(LogLevel level, const String& component, const String& message) {
    String formatted = "";
    
    if (timestamps_enabled) {
        formatted += "[" + getTimestamp() + "] ";
    }
    
    formatted += "[" + logLevelToString(level) + "] ";
    formatted += "[" + component + "] ";
    formatted += message;
    
    return formatted;
}

String UnifiedLogger::logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:    return "DEBUG";
        case LogLevel::INFO:     return "INFO";
        case LogLevel::WARNING:  return "WARN";
        case LogLevel::ERROR:    return "ERROR";
        case LogLevel::CRITICAL: return "CRIT";
        default:                 return "UNKNOWN";
    }
}

UnifiedLogger& UnifiedLogger::getInstance() {
    static UnifiedLogger instance;
    return instance;
}

void UnifiedLogger::outputMessage(const String& formatted_message) {
    if (serial_output_enabled) {
        writeToSerial(formatted_message);
    }
    
    if (web_output_enabled && WiFi.status() == WL_CONNECTED) {
        writeToWeb(formatted_message);
    }
}

String UnifiedLogger::getTimestamp() {
    unsigned long ms = millis();
    unsigned long seconds = ms / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    
    ms %= 1000;
    seconds %= 60;
    minutes %= 60;
    hours %= 24;
    
    char timestamp[16];
    snprintf(timestamp, sizeof(timestamp), "%02lu:%02lu:%02lu.%03lu", 
             hours, minutes, seconds, ms);
    
    return String(timestamp);
}

void UnifiedLogger::writeToSerial(const String& message) {
    if (Serial) {
        Serial.println(message);
    }
}

void UnifiedLogger::writeToWeb(const String& message) {
    // Send to WebSerial if available
    WebSerial.println(message);
}