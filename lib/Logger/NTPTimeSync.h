#ifndef NTP_TIME_SYNC_H
#define NTP_TIME_SYNC_H

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <functional>

/**
 * NTP Time Synchronization Manager
 * Handles automatic time synchronization across ESP-NOW mesh network
 */
class NTPTimeSync {
private:
    // NTP Configuration
    static const char* NTP_SERVER_1;
    static const char* NTP_SERVER_2;
    static const char* NTP_SERVER_3;
    static const char* TIMEZONE_MEXICO;
    
    // Geographic configuration
    String customTimezone;
    String countryCode;
    String cityName;
    bool autoGeoLocation;
    
    // State management
    bool isInitialized;
    bool isTimeSynced;
    bool hasGeoLocation;
    unsigned long lastSyncAttempt;
    unsigned long lastSyncSuccess;
    unsigned long lastGeoAttempt;
    static const unsigned long SYNC_INTERVAL = 3600000; // 1 hour
    static const unsigned long SYNC_RETRY_INTERVAL = 60000; // 1 minute
    static const unsigned long GEO_RETRY_INTERVAL = 300000; // 5 minutes
    
    // Callbacks
    std::function<void(bool)> onSyncCallback;
    std::function<void(bool, String, String)> onGeoCallback;
    
    // Internal methods
    bool attemptSync();
    void configureTimezone();
    bool attemptGeoLocation();
    String getTimezoneFromLocation(const String& country, const String& city);
    
public:
    NTPTimeSync();
    
    // Initialization
    bool begin();
    void loop();
    
    // Time synchronization
    bool syncNow();
    bool isReady() const { return isTimeSynced; }
    unsigned long getLastSyncTime() const { return lastSyncSuccess; }
    
    // Geographic configuration
    void setTimezone(const String& timezone);
    void setLocation(const String& countryCode, const String& city);
    void enableAutoGeoLocation(bool enabled);
    bool hasValidGeoLocation() const { return hasGeoLocation; }
    String getDetectedLocation() const;
    
    // Time utilities
    uint32_t getCurrentTimestamp() const;
    String getCurrentTimeString() const;
    String getCurrentDateString() const;
    String getCurrentDateTimeString() const;
    String getTimeWithMsString() const;   // "14:30:45.123" for logs
    String getFormattedTime(const char* format) const;
    
    // Compact display formats
    String getCompactTime() const;      // "14:30" for OLED
    String getCompactDate() const;      // "27/09" for OLED
    String getCompactDateTime() const;  // "27/09 14:30" for OLED
    String getFormattedTime(uint32_t timestamp) const;
    
    // For ESP-NOW mesh synchronization
    struct TimeInfo {
        uint32_t timestamp;
        uint32_t masterTimestamp;
        int8_t timezoneOffset;
        bool isValid;
    };
    
    TimeInfo getTimeInfo() const;
    bool setTimeFromMaster(const TimeInfo& timeInfo);
    
    // Callbacks
    void onTimeSync(std::function<void(bool)> callback) { onSyncCallback = callback; }
    
    // Status
    String getStatusString() const;
    bool needsSync() const;
    
    // Offline time estimation (for slaves without internet)
    void startLocalClock();
    uint32_t getEstimatedTimestamp() const;
    bool isUsingEstimatedTime() const;
};

#endif // NTP_TIME_SYNC_H