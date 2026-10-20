#include "HardwareBoardStatus.h"
#include "../DisplayManager/DisplayManager.h"
#include "../Config/ApplicationConfig.h"

HardwareBoardStatus::HardwareBoardStatus(DisplayManager* displayManager, ApplicationConfig* configManager) : 
_displayManager(displayManager), _configManager(configManager) {
}

void HardwareBoardStatus::begin() {
    _oledStartTime = millis();
}

void HardwareBoardStatus::loop() {
}

bool HardwareBoardStatus::isMonitorEnabled() const {
    return _monitorEnabled;
}

void HardwareBoardStatus::setMonitorEnabled(bool enabled) {
    _monitorEnabled = enabled;
}

unsigned long HardwareBoardStatus::getOledStartTime() const {
    return _oledStartTime;
}

void HardwareBoardStatus::setOledStartTime(unsigned long time) {
    _oledStartTime = time;
}

unsigned long HardwareBoardStatus::getOledTimeout() const {
    return _oledTimeout;
}

void HardwareBoardStatus::setDisplayEnabled(bool enabled) {
    if (_displayManager) {
        if (enabled) {
            _displayManager->displayOn();
        } else {
            _displayManager->displayOff();
        }
    }
}

void HardwareBoardStatus::setUpdating(bool updating) {
    _isUpdating = updating;
}

bool HardwareBoardStatus::isUpdating() const {
    return _isUpdating;
}

void HardwareBoardStatus::resetOledTimeout() {
    _oledStartTime = millis();
}

bool HardwareBoardStatus::isDisplayEnabled() const {
    // No hay una forma directa de verificar si el display está encendido
    // Retornamos true por defecto ya que normalmente está activo
    return true;
}