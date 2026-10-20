#ifndef LIQUID_LEVEL_SENSOR_H
#define LIQUID_LEVEL_SENSOR_H

#include "Sensor.h"
#include <HCSR04.h>
#include "../Config/ApplicationConfig.h"

class LiquidLevelSensor : public Sensor {
public:
    LiquidLevelSensor(uint8_t triggerPin, uint8_t echoPin, ApplicationConfig* configManager);

    // Sensor interface implementation
    bool begin() override;
    void update() override;
    String getName() const override;
    String getJson() const override;
    
    // ISensor interface overrides for better control
    bool hasValidReading() const override { return m_distanciaCm >= 0; }
    float getDistance() const override { return m_distanciaCm; }
    float getPercentage() const override;
    float getLitros() const;  // Fórmula única: capacidad * porcentaje / 100 (0 si lectura inválida)
    String getLastError() const override { return m_mensajeError; }
    bool isConnected() const override { return m_distanciaCm >= 0; }
    
    // Estabilidad del sensor (detección de ecos falsos/multipath)
    bool isSensorStable() const { return !m_sensorUnstable; }
    bool isFailsafeActive() const { return m_failsafeActive; }

    // Máximo de muestras soportado (arrays estáticos)
    static const uint8_t MAX_SIGMA_SAMPLES = 30;
    
    // Modo instalación: lectura cruda rápida (mediana de pocas muestras), sin spike guard,
    // sigma ni estabilidad, y sin modificar el estado del filtro. -1 si no hay eco.
    float measureRawQuick(uint8_t samples = RAW_QUICK_SAMPLES);
    // Borra el estado del filtro (historial, spike guard, failsafe) tras mover el sensor.
    // seedDistanceCm: última distancia cruda medida (referencia inicial), o -1.
    void resetFilterState(float seedDistanceCm);
    static const uint8_t RAW_QUICK_SAMPLES = 5;

    // Acceso a detalles de la última medición (para monitor verbose)
    String getLastMeasurementDetails() const;

    // Getters para datos diagnósticos (endpoint /api/last-readings)
    uint8_t getLastTotalSamples() const { return m_lastTotalSamples; }
    const float* getLastRawSamples() const { return m_lastRawSamples; }
    const bool* getLastSpikeFlags() const { return m_lastSpikeFlags; }
    const bool* getLastSigmaFlags() const { return m_lastSigmaFlags; }
    float getLastMean() const { return m_lastMean; }
    float getLastStdDev() const { return m_lastStdDev; }
    uint8_t getLastSurvivorCount() const { return m_lastSurvivorCount; }
    float getLastFinalAvg() const { return m_lastFinalAvg; }
    uint8_t getLastRetryCount() const { return m_lastRetryCount; }
    uint8_t getLastRetryRecovered() const { return m_lastRetryRecovered; }

private:
    UltraSonicDistanceSensor m_distanceSensor;
    uint8_t m_echoPin;
    static constexpr float MIN_VALID_CM = 1.0f;  // Ningún ultrasónico mide < ~2 cm
    static constexpr uint16_t RAW_SAMPLE_DELAY_MS = 60; // Ciclo mínimo HC-SR04 / AJ-SR04M
    float measureOnce();
    ApplicationConfig* m_configManager;
    float m_distanciaCm;
    float m_lastValidDistanceCm;    // Última distancia válida (para recuperación en rango mínimo)
    uint8_t m_consecutiveErrors;    // Contador de errores consecutivos del sensor
    String m_mensajeError;
    
    // Detalles de última medición para diagnóstico (filtro sigma)
    float m_lastRawSamples[MAX_SIGMA_SAMPLES];   // Todas las muestras RAW tomadas
    uint8_t m_lastTotalSamples;       // Total de muestras tomadas
    uint8_t m_lastValidCount;         // Cuántas fueron válidas (0-400cm)
    uint8_t m_lastSpikeRejected;      // Muestras rechazadas por spike guard
    bool  m_lastSpikeFlags[MAX_SIGMA_SAMPLES];   // Qué muestras fueron spike
    float m_lastMean;                 // Media (μ) de muestras pre-sigma
    float m_lastStdDev;              // Desviación estándar (σ)
    uint8_t m_lastSigmaRejected;     // Muestras rechazadas por sigma
    bool  m_lastSigmaFlags[MAX_SIGMA_SAMPLES];   // Qué muestras fueron rechazadas por sigma
    float m_lastSurvivors[MAX_SIGMA_SAMPLES];    // Muestras supervivientes
    uint8_t m_lastSurvivorCount;     // Cuántas sobrevivieron
    float m_lastFinalAvg;            // Promedio final
    uint8_t m_lastRetryCount;        // Muestras sin eco que se reintentaron
    uint8_t m_lastRetryRecovered;    // Reintentos que sí obtuvieron eco
    static constexpr uint16_t SAMPLE_RETRY_DELAY_MS = 60;  // HC-SR04/JSN-SR04T: ciclo mínimo ~60ms

    // Spike guard: protección contra deadlock
    uint8_t m_consecutiveSpikeRejects; // Ciclos consecutivos donde spike rechazó la mayoría
    
    // --- Detección de estabilidad (ecos falsos / multipath) ---
    // Si las últimas N lecturas oscilan mucho (rango alto + muchos cambios de
    // dirección), el sensor probablemente está en zona muerta y las lecturas
    // son ecos secundarios de las paredes del contenedor.
    static constexpr uint8_t HISTORY_SIZE = 10;
    static constexpr float UNSTABLE_RANGE_CM = 8.0f;        // Rango en historial > esto = inestable
    static constexpr float UNSTABLE_OSC_RANGE_CM = 5.0f;    // Rango mínimo para que oscilaciones cuenten
    static constexpr uint8_t UNSTABLE_OSCILLATION_MIN = 4;  // Cambios de dirección >= esto = oscilando
    static constexpr uint8_t UNSTABLE_COUNT_FAILSAFE = 10;  // Lecturas inestables consecutivas para failsafe
    static constexpr uint8_t STABLE_COOLDOWN_COUNT = 5;     // Lecturas estables consecutivas para limpiar flag
    
    float m_readingHistory[10];       // Últimas N lecturas aceptadas (buffer circular)
    uint8_t m_historyIndex;
    uint8_t m_historyCount;
    uint8_t m_consecutiveUnstable;    // Lecturas inestables consecutivas
    uint8_t m_consecutiveStable;      // Lecturas estables consecutivas (para histéresis)
    bool m_sensorUnstable;            // Estado actual de estabilidad
    bool m_failsafeActive;            // Failsafe activado (asumiendo lleno)

    String calculateLitros() const;
    float measureSigmaFiltered();  // Filtro sigma: N muestras, rechaza outliers, promedia supervivientes
    void analyzeStability(float& range, uint8_t& oscillations) const;
};

#endif // LIQUID_LEVEL_SENSOR_H