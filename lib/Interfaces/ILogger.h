#ifndef ILOGGER_H
#define ILOGGER_H

#include <Arduino.h>

/**
 * @brief Enumeration for log levels
 */
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4
};

/**
 * @brief Interface for logging system
 * 
 * This interface provides a unified logging system that can output
 * to different destinations (Serial, WebSerial, File, etc.)
 */
class ILogger {
public:
    virtual ~ILogger() = default;
    
    // Core logging operations
    virtual bool begin() = 0;
    virtual void setLogLevel(LogLevel level) = 0;
    virtual LogLevel getLogLevel() const = 0;
    
    // Logging methods
    virtual void debug(const String& message, const String& component = "") = 0;
    virtual void info(const String& message, const String& component = "") = 0;
    virtual void warning(const String& message, const String& component = "") = 0;
    virtual void error(const String& message, const String& component = "") = 0;
    virtual void critical(const String& message, const String& component = "") = 0;
    
    // Formatted logging
    virtual void logf(LogLevel level, const String& component, const char* format, ...) = 0;
    
    // Configuration
    virtual void setComponent(const String& component) = 0;
    virtual void enableTimestamps(bool enable) = 0;
    virtual void enableWebOutput(bool enable) = 0;
    virtual void enableSerialOutput(bool enable) = 0;
    
    // Status
    virtual bool isEnabled() const = 0;
    virtual void setEnabled(bool enabled) = 0;
    
    // Utility methods
    virtual String formatMessage(LogLevel level, const String& component, const String& message) = 0;
    virtual String logLevelToString(LogLevel level) = 0;
};

#endif // ILOGGER_H