#ifndef OTA_UPDATER_H
#define OTA_UPDATER_H

#include <Arduino.h>
#include <ElegantOTA.h>
#include "DisplayManager.h"
#include "HardwareBoardStatus.h"

class OTAUpdater {
public:
    OTAUpdater(DisplayManager* displayManager, HardwareBoardStatus* systemStatus);
    void begin(AsyncWebServer* server);
    void loop();

private:
    DisplayManager* _displayManager;
    HardwareBoardStatus* _systemStatus;
};

#endif // OTA_UPDATER_H
