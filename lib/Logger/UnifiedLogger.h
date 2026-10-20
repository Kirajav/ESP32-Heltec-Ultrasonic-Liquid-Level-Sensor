#ifndef UNIFIED_LOGGER_H
#define UNIFIED_LOGGER_H

#include "ILogger.h"
#include <WebSerial.h>

/**
 * @brief Unified Logger Implementation
 * 
 * This class implements a centralized logging system that can output
 * to multiple destinations: Serial, WebSerial, and potentially files.
 * It provides structured logging with timestamps, components, and levels.
 */
class UnifiedLogger : public ILogger {
public:
    UnifiedLogger();
    
    // ILogger implementation
    bool begin() override;
    void setLogLevel(LogLevel level) override;
    LogLevel getLogLevel() const override;
    
    // Logging methods
    void debug(const String& message, const String& component = "") override;
    void info(const String& message, const String& component = "") override;
    void warning(const String& message, const String& component = "") override;
    void error(const String& message, const String& component = "") override;
    void critical(const String& message, const String& component = "") override;
    
    // Formatted logging
    void logf(LogLevel level, const String& component, const char* format, ...) override;
    
    // Configuration
    void setComponent(const String& component) override;
    void enableTimestamps(bool enable) override;
    void enableWebOutput(bool enable) override;
    void enableSerialOutput(bool enable) override;
    
    // Status
    bool isEnabled() const override;
    void setEnabled(bool enabled) override;
    
    // Utility methods
    String formatMessage(LogLevel level, const String& component, const String& message) override;
    String logLevelToString(LogLevel level) override;
    
    // Static instance for global access
    static UnifiedLogger& getInstance();
    
private:
    LogLevel current_log_level;
    String default_component;
    bool timestamps_enabled;
    bool web_output_enabled;
    bool serial_output_enabled;
    bool is_enabled;
    
    void outputMessage(const String& formatted_message);
    String getTimestamp();
    void writeToSerial(const String& message);
    void writeToWeb(const String& message);
};

// Global logger instance access
extern UnifiedLogger& Logger;

// Convenience macros for easier logging
#define LOG_DEBUG(msg, ...) Logger.debug(String(msg), ##__VA_ARGS__)
#define LOG_INFO(msg, ...) Logger.info(String(msg), ##__VA_ARGS__)
#define LOG_WARNING(msg, ...) Logger.warning(String(msg), ##__VA_ARGS__)
#define LOG_ERROR(msg, ...) Logger.error(String(msg), ##__VA_ARGS__)
#define LOG_CRITICAL(msg, ...) Logger.critical(String(msg), ##__VA_ARGS__)

// Component-specific logging macros
#define LOG_COMPONENT_DEBUG(component, msg) Logger.debug(String(msg), String(component))
#define LOG_COMPONENT_INFO(component, msg) Logger.info(String(msg), String(component))
#define LOG_COMPONENT_WARNING(component, msg) Logger.warning(String(msg), String(component))
#define LOG_COMPONENT_ERROR(component, msg) Logger.error(String(msg), String(component))
#define LOG_COMPONENT_CRITICAL(component, msg) Logger.critical(String(msg), String(component))

#endif // UNIFIED_LOGGER_H