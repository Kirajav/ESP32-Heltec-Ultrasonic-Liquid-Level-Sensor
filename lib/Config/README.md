# 🔧 Configuration Management Module

## 📁 Structure

```
lib/Config/
├── ApplicationConfig.h/cpp          # Main configuration facade
├── Domain/                          # Domain-specific configurations
│   ├── NetworkConfig.h/cpp         # WiFi, hostname, network settings
│   ├── SensorConfig.h/cpp          # Sensor calibration, thresholds
│   ├── ESPNowConfig.h/cpp          # ESP-NOW mesh network config
│   └── HardwareBoardConfig.h/cpp   # Hardware-specific settings
└── Persistence/                     # Data persistence (future)
    └── (Reserved for storage abstractions)
```

## 🎯 Design Patterns

### **Facade Pattern**
`ApplicationConfig` acts as a unified interface to all configuration domains.

### **Domain-Driven Design**
Each domain handles its specific configuration concerns:
- **Network**: WiFi credentials, connection settings
- **Sensor**: Calibration values, measurement intervals
- **Hardware**: Display settings, pin configurations
- **ESP-NOW**: Mesh network, peer management

### **Single Responsibility Principle**
Each configuration class has one reason to change.

## 📝 Usage

```cpp
#include "Config/ApplicationConfig.h"

ApplicationConfig config;
config.begin();

// Access domain-specific settings
auto& network = config.getNetworkConfig();
auto& sensor = config.getSensorConfig();

// Save changes
config.save();
```

## 🔄 Migration Notes

- **Former**: `ConfigManagerV2` (technical naming)
- **Current**: `ApplicationConfig` (business-oriented naming)
- All references updated throughout the codebase
- Maintains backward compatibility in functionality