#ifndef HARDWARE_BOARD_STATUS_H
#define HARDWARE_BOARD_STATUS_H

#include <Arduino.h>

class DisplayManager;
class ApplicationConfig;

class HardwareBoardStatus {
public:
    HardwareBoardStatus(DisplayManager* displayManager, ApplicationConfig* configManager);
    void begin();
    void loop();
    bool isMonitorEnabled() const;
    void setMonitorEnabled(bool enabled);
    unsigned long getOledStartTime() const;
    void setOledStartTime(unsigned long time);
    unsigned long getOledTimeout() const;
    void setDisplayEnabled(bool enabled);
    void setUpdating(bool updating);
    bool isUpdating() const;
    void resetOledTimeout();
    bool isDisplayEnabled() const;

private:
    DisplayManager* _displayManager;
    ApplicationConfig* _configManager;
    
    bool _monitorEnabled = false;
    unsigned long _oledStartTime = 0;
    const long _oledTimeout = 300000;   
    bool _isUpdating = false;
};

#endif // HARDWARE_BOARD_STATUS_H