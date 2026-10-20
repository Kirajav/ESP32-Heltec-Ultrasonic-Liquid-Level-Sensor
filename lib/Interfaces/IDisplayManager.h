#ifndef IDISPLAY_MANAGER_H
#define IDISPLAY_MANAGER_H

#include <Arduino.h>

/**
 * @brief Interface for display management
 * 
 * This interface abstracts display operations, allowing for different
 * display implementations (OLED, LCD, etc.) and facilitating testing.
 */
class IDisplayManager {
public:
    virtual ~IDisplayManager() = default;
    
    // Core display operations
    virtual bool begin() = 0;
    virtual void clear() = 0;
    virtual void display() = 0;
    virtual void displayOff() = 0;
    virtual void displayOn() = 0;
    
    // Text operations
    virtual void drawString(int x, int y, const String& text) = 0;
    virtual void setFont(const char* font) = 0;
    virtual void setTextAlignment(int alignment) = 0;
    
    // Status management
    virtual void handleDisplayStateChange(bool enabled) = 0;
    virtual bool isDisplayEnabled() const = 0;
    
    // Auto-sleep control
    virtual void setAutoSleepTime(uint16_t seconds) = 0;
    virtual uint16_t getAutoSleepTime() const = 0;
    virtual void resetAutoSleepTimer() = 0;
    virtual void checkAutoSleep() = 0;
    
    // Display information
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
    virtual String getDisplayType() const = 0;
    
    // Error handling
    virtual bool isConnected() const = 0;
    virtual String getLastError() const = 0;
};

#endif // IDISPLAY_MANAGER_H