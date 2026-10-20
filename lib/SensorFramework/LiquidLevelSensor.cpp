#include "LiquidLevelSensor.h"
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

LiquidLevelSensor::LiquidLevelSensor(uint8_t triggerPin, uint8_t echoPin, ApplicationConfig* configManager)
    : m_distanceSensor(triggerPin, echoPin), m_configManager(configManager), m_distanciaCm(-1.0),
      m_lastValidDistanceCm(-1.0), m_consecutiveErrors(0), m_mensajeError("No_init"),
      m_lastTotalSamples(0), m_lastValidCount(0), m_lastSpikeRejected(0),
      m_lastMean(0), m_lastStdDev(0), m_lastSigmaRejected(0),
      m_lastSurvivorCount(0), m_lastFinalAvg(-1),
      m_lastRetryCount(0), m_lastRetryRecovered(0),
      m_consecutiveSpikeRejects(0),
      m_historyIndex(0), m_historyCount(0), m_consecutiveUnstable(0),
      m_consecutiveStable(0), m_sensorUnstable(false), m_failsafeActive(false) {
    memset(m_lastRawSamples, 0, sizeof(m_lastRawSamples));
    memset(m_lastSpikeFlags, 0, sizeof(m_lastSpikeFlags));
    memset(m_lastSigmaFlags, 0, sizeof(m_lastSigmaFlags));
    memset(m_lastSurvivors, 0, sizeof(m_lastSurvivors));
    memset(m_readingHistory, 0, sizeof(m_readingHistory));
}

bool LiquidLevelSensor::begin() {
    // La librería HCSR04 no requiere un método begin(),
    // la inicialización se hace en el constructor.
    m_mensajeError = "OK";
    return true;
}

void LiquidLevelSensor::update() {
    float medida = measureSigmaFiltered();
    // Un rango de 4m es el máximo para este sensor
    if (medida >= 0 && medida <= 400) { 
        // --- Agregar al historial circular ---
        m_readingHistory[m_historyIndex] = medida;
        m_historyIndex = (m_historyIndex + 1) % HISTORY_SIZE;
        if (m_historyCount < HISTORY_SIZE) m_historyCount++;
        
        // --- Análisis de estabilidad ---
        // Detectar ecos falsos/multipath: lecturas oscilantes con rango alto
        // Esto ocurre cuando el agua está en la zona muerta del sensor y
        // los ecos rebotan en las paredes del contenedor.
        if (m_historyCount >= 5) {
            float range = 0;
            uint8_t oscillations = 0;
            analyzeStability(range, oscillations);
            
            bool currentlyUnstable = (range > UNSTABLE_RANGE_CM && 
                                      oscillations >= UNSTABLE_OSCILLATION_MIN) ||
                                     (oscillations >= UNSTABLE_OSCILLATION_MIN && 
                                      range > UNSTABLE_OSC_RANGE_CM);
            
            if (currentlyUnstable) {
                m_consecutiveUnstable++;
                m_consecutiveStable = 0;  // Reset cooldown
                m_sensorUnstable = true;
                
                // Failsafe: tras muchas lecturas inestables consecutivas,
                // asumir tanque lleno para evitar desbordamiento
                if (m_consecutiveUnstable >= UNSTABLE_COUNT_FAILSAFE && 
                    m_historyCount >= HISTORY_SIZE) {
                    float minDist = m_configManager->getMinDistance();
                    m_distanciaCm = minDist;
                    m_failsafeActive = true;
                    m_mensajeError = "Lleno (seguridad)";
                    Serial.printf("[SENSOR] ⚠️ FAILSAFE: %d lecturas inestables "
                                 "(rango=%.1fcm, osc=%d) -> Asumiendo LLENO (%.1fcm)\n",
                                 m_consecutiveUnstable, range, oscillations, minDist);
                    return;
                }
            } else {
                // Histéresis: solo limpiar flag inestable tras N lecturas estables consecutivas
                if (m_sensorUnstable) {
                    m_consecutiveStable++;
                    if (m_consecutiveStable >= STABLE_COOLDOWN_COUNT) {
                        m_sensorUnstable = false;
                        m_failsafeActive = false;
                        m_consecutiveUnstable = 0;
                        m_consecutiveStable = 0;
                        Serial.println("[SENSOR] ✅ Sensor estable de nuevo tras cooldown");
                    }
                    // Mientras está en cooldown, mantener flag inestable
                } else {
                    m_consecutiveUnstable = 0;
                }
            }
        }
        
        // Lectura aceptada
        m_distanciaCm = medida;
        m_lastValidDistanceCm = medida;
        m_consecutiveErrors = 0;
        if (!m_sensorUnstable) m_failsafeActive = false;
        m_mensajeError = m_sensorUnstable ? "Inestable" : "OK";
    } else {
        m_consecutiveErrors++;
        // Cuando el sensor retorna -1 y la última lectura válida estaba cerca
        // de la distancia mínima, el tanque está lleno y el agua está demasiado
        // cerca del sensor (< rango mínimo ~20cm). Tratar como "tanque lleno".
        float minDist = m_configManager->getMinDistance();
        if (m_lastValidDistanceCm >= 0 && m_lastValidDistanceCm <= minDist * 1.5) {
            m_distanciaCm = minDist;  // Clampar a distancia mínima = tanque lleno
            m_mensajeError = "Lleno (min)";
            Serial.printf("[SENSOR] Sin eco, ultima valida=%.1fcm <= %.1fcm -> LLENO\n",
                         m_lastValidDistanceCm, minDist * 1.5);
        } else if (m_sensorUnstable || m_failsafeActive) {
            // Si venía inestable y ahora pierde el eco, asumir lleno
            m_distanciaCm = minDist;
            m_failsafeActive = true;
            m_mensajeError = "Lleno (seguridad)";
            Serial.printf("[SENSOR] Sin eco tras inestabilidad -> LLENO\n");
        } else {
            m_distanciaCm = -1.0; // Valor inválido genuino
            m_mensajeError = "Error de lectura";
        }
    }
}

// ===========================================================
// Analiza estabilidad del historial de lecturas.
// Calcula rango (max-min) y cuenta oscilaciones (cambios de 
// dirección). Lecturas estables tienen rango bajo y pocas
// oscilaciones. Ecos falsos tienen rango alto + muchas oscilaciones.
// ===========================================================
void LiquidLevelSensor::analyzeStability(float& range, uint8_t& oscillations) const {
    float hMin = 9999.0f, hMax = -1.0f;
    oscillations = 0;
    
    // Calcular rango del historial
    for (uint8_t i = 0; i < m_historyCount; i++) {
        if (m_readingHistory[i] < hMin) hMin = m_readingHistory[i];
        if (m_readingHistory[i] > hMax) hMax = m_readingHistory[i];
    }
    range = hMax - hMin;
    
    // Contar cambios de dirección (oscilaciones)
    // Recorrer el historial en orden cronológico usando el buffer circular
    for (uint8_t i = 2; i < m_historyCount; i++) {
        uint8_t idx0 = (m_historyIndex + HISTORY_SIZE - m_historyCount + i - 2) % HISTORY_SIZE;
        uint8_t idx1 = (idx0 + 1) % HISTORY_SIZE;
        uint8_t idx2 = (idx1 + 1) % HISTORY_SIZE;
        float d1 = m_readingHistory[idx1] - m_readingHistory[idx0];
        float d2 = m_readingHistory[idx2] - m_readingHistory[idx1];
        // Cambio de dirección con deadband de 2cm (evitar ruido normal)
        if ((d1 > 2.0f && d2 < -2.0f) || (d1 < -2.0f && d2 > 2.0f)) {
            oscillations++;
        }
    }
}

// ===========================================================
// Filtro Sigma: toma N muestras, aplica spike guard (rechaza
// muestras con desviación extrema de la última lectura válida),
// calcula μ y σ, rechaza muestras fuera de μ±kσ, y promedia
// las supervivientes. Produce lecturas mucho más estables que
// el simple recorte min/max anterior.
// Parámetros configurables desde dashboard/portal:
//   sigma_samples    (5-30, default 20)
//   sigma_delay_ms   (10-500, default 300)
//   sigma_threshold  (1.0-3.0, default 1.5)
//   sigma_min_valid  (3-20, default 10)
//
// Spike Guard Deadlock Protection:
//   Si el spike guard rechaza la mayoría de muestras durante 3
//   ciclos consecutivos, resetea m_lastValidDistanceCm para
//   aceptar el nuevo nivel real (ej: cambio >30cm por consumo
//   masivo o llenado mientras el sistema estaba apagado).
// ===========================================================
float LiquidLevelSensor::measureSigmaFiltered() {
    const auto& sensorCfg = m_configManager->getSensorConfig();
    const uint8_t numSamples = sensorCfg.getSigmaSamples();
    const uint16_t delayMs = sensorCfg.getSigmaDelayMs();
    const float sigmaK = sensorCfg.getSigmaThreshold();
    const uint8_t minValid = sensorCfg.getSigmaMinValid();
    
    // Limitar a MAX_SIGMA_SAMPLES
    const uint8_t N = (numSamples > MAX_SIGMA_SAMPLES) ? MAX_SIGMA_SAMPLES : numSamples;
    
    // Reset diagnóstico
    m_lastTotalSamples = N;
    m_lastValidCount = 0;
    m_lastSpikeRejected = 0;
    m_lastSigmaRejected = 0;
    m_lastSurvivorCount = 0;
    m_lastMean = 0;
    m_lastStdDev = 0;
    m_lastFinalAvg = -1;
    m_lastRetryCount = 0;
    m_lastRetryRecovered = 0;
    memset(m_lastSpikeFlags, 0, sizeof(m_lastSpikeFlags));
    memset(m_lastSigmaFlags, 0, sizeof(m_lastSigmaFlags));
    
    // === FASE 1: Tomar muestras RAW ===
    float validSamples[MAX_SIGMA_SAMPLES];
    uint8_t validCount = 0;
    
    for (uint8_t i = 0; i < N; i++) {
        float raw = m_distanceSensor.measureDistanceCm();
        if (raw < 0) {
            // Reintento único: se observó un patrón alterno sin eco (E,O,E,O...).
            // rt/rr en /api/last-readings dicen si el fallo es por disparo o persistente
            m_lastRetryCount++;
            vTaskDelay(pdMS_TO_TICKS(SAMPLE_RETRY_DELAY_MS));
            raw = m_distanceSensor.measureDistanceCm();
            if (raw >= 0) m_lastRetryRecovered++;
        }
        m_lastRawSamples[i] = raw;
        
        if (raw >= 0 && raw <= 400) {
            // === SPIKE GUARD ===
            // Rechazar muestras que difieren >30cm de la última lectura válida
            // (detecta ecos fantasma del HC-SR04 en tanques cilíndricos)
            if (m_lastValidDistanceCm >= 0 && fabsf(raw - m_lastValidDistanceCm) > 30.0f) {
                m_lastSpikeFlags[i] = true;
                m_lastSpikeRejected++;
            } else {
                validSamples[validCount++] = raw;
            }
            m_lastValidCount++;
        }
        
        if (i < N - 1) {
            vTaskDelay(pdMS_TO_TICKS(delayMs));
        }
    }
    
    // === SPIKE GUARD DEADLOCK PROTECTION ===
    // Si el spike guard rechazó la mayoría de muestras en-rango,
    // el nivel real probablemente cambió (ej: llenado/consumo masivo
    // mientras el sistema estaba apagado). Tras 3 ciclos consecutivos
    // así, reseteamos la referencia para aceptar el nuevo nivel.
    if (m_lastValidCount > 0 && m_lastSpikeRejected > m_lastValidCount / 2) {
        m_consecutiveSpikeRejects++;
        if (m_consecutiveSpikeRejects >= 3) {
            Serial.println("[SENSOR] ⚠️ Spike guard reset: nivel cambió significativamente, aceptando nueva referencia");
            m_lastValidDistanceCm = -1.0f;
            m_consecutiveSpikeRejects = 0;
            // Reintentar la medición sin spike guard (recursión única)
            return measureSigmaFiltered();
        }
    } else {
        m_consecutiveSpikeRejects = 0;
    }
    
    // Si no hay suficientes muestras válidas post-spike, error
    if (validCount < 2) {
        if (validCount == 1) {
            m_lastFinalAvg = validSamples[0];
            m_lastSurvivors[0] = validSamples[0];
            m_lastSurvivorCount = 1;
            return validSamples[0];
        }
        return -1.0f;
    }
    
    // === FASE 2: Calcular μ (media) y σ (desviación estándar) ===
    float sum = 0;
    for (uint8_t i = 0; i < validCount; i++) {
        sum += validSamples[i];
    }
    float mean = sum / validCount;
    
    float sumSqDiff = 0;
    for (uint8_t i = 0; i < validCount; i++) {
        float diff = validSamples[i] - mean;
        sumSqDiff += diff * diff;
    }
    float stddev = sqrtf(sumSqDiff / validCount);
    
    m_lastMean = mean;
    m_lastStdDev = stddev;
    
    // === FASE 3: Rechazar muestras fuera de μ ± k*σ ===
    float lowerBound = mean - sigmaK * stddev;
    float upperBound = mean + sigmaK * stddev;
    
    uint8_t survivorCount = 0;
    float survivorSum = 0;
    
    // Mapear índices de validSamples a muestras RAW para flags de diagnóstico
    uint8_t rawValidIdx = 0;
    for (uint8_t rawIdx = 0; rawIdx < N; rawIdx++) {
        // Saltar muestras que fueron inválidas (<0 o >400) o spike
        if (m_lastRawSamples[rawIdx] < 0 || m_lastRawSamples[rawIdx] > 400 || m_lastSpikeFlags[rawIdx]) {
            continue;
        }
        
        float val = validSamples[rawValidIdx];
        rawValidIdx++;
        
        if (val < lowerBound || val > upperBound) {
            m_lastSigmaFlags[rawIdx] = true;
            m_lastSigmaRejected++;
        } else {
            m_lastSurvivors[survivorCount++] = val;
            survivorSum += val;
        }
    }
    
    m_lastSurvivorCount = survivorCount;
    
    // === FASE 4: Resultado final ===
    if (survivorCount >= minValid) {
        m_lastFinalAvg = survivorSum / survivorCount;
        return m_lastFinalAvg;
    }
    
    // No hay suficientes supervivientes pero sí hay datos válidos:
    // usar todas las muestras válidas (sin filtro sigma) como fallback
    if (validCount >= minValid) {
        m_lastFinalAvg = mean;
        m_lastSurvivorCount = validCount;
        return mean;
    }
    
    // Último recurso: usar lo que haya si hay al menos 2 muestras
    if (survivorCount >= 2) {
        m_lastFinalAvg = survivorSum / survivorCount;
        return m_lastFinalAvg;
    }
    if (validCount >= 2) {
        m_lastFinalAvg = mean;
        return mean;
    }
    
    return -1.0f;
}

// ===========================================================
// Devuelve un string con los detalles completos de la última
// medición con filtro sigma para diagnóstico (WebSerial monitor).
// ===========================================================
String LiquidLevelSensor::getLastMeasurementDetails() const {
    String s = "";
    
    // 1. Lecturas RAW
    s += "--- Mediciones RAW (" + String(m_lastTotalSamples) + " muestras) ---\n";
    for (uint8_t i = 0; i < m_lastTotalSamples; i++) {
        s += "  #" + String(i + 1) + ": ";
        if (m_lastRawSamples[i] < 0) {
            s += "ERROR (sin eco)\n";
        } else {
            s += String(m_lastRawSamples[i], 2) + " cm";
            if (m_lastSpikeFlags[i]) {
                s += " [SPIKE]";
            } else if (m_lastSigmaFlags[i]) {
                s += " [SIGMA]";
            } else if (m_lastRawSamples[i] >= 0 && m_lastRawSamples[i] <= 400) {
                s += " OK";
            }
            s += "\n";
        }
    }
    
    // 2. Resumen del filtro sigma
    s += "--- Filtro Sigma ---\n";
    s += "  Validas (0-400cm): " + String(m_lastValidCount) + "/" + String(m_lastTotalSamples) + "\n";
    s += "  Reintentos sin eco: " + String(m_lastRetryCount) + " (recuperadas: " + String(m_lastRetryRecovered) + ")\n";
    s += "  Spike guard (>30cm): " + String(m_lastSpikeRejected) + " rechazadas\n";
    uint8_t preSigma = m_lastValidCount - m_lastSpikeRejected;
    s += "  Pre-sigma: " + String(preSigma) + " muestras\n";
    s += "  Media (μ): " + String(m_lastMean, 2) + " cm\n";
    s += "  Desv. est. (σ): " + String(m_lastStdDev, 3) + " cm\n";
    float sigmaK = m_configManager->getSensorConfig().getSigmaThreshold();
    s += "  Rango aceptado: " + String(m_lastMean - sigmaK * m_lastStdDev, 2) 
       + " - " + String(m_lastMean + sigmaK * m_lastStdDev, 2) + " cm (μ±" + String(sigmaK, 1) + "σ)\n";
    s += "  Sigma rechazadas: " + String(m_lastSigmaRejected) + "\n";
    s += "  Supervivientes: " + String(m_lastSurvivorCount) + "\n";
    if (m_lastSurvivorCount > 0) {
        for (uint8_t i = 0; i < m_lastSurvivorCount; i++) {
            s += "    [" + String(i + 1) + "] " + String(m_lastSurvivors[i], 2) + " cm\n";
        }
    }
    s += "  Promedio final: " + String(m_lastFinalAvg, 2) + " cm\n";
    
    // 3. Cálculos de nivel
    s += "--- Calculo de Nivel ---\n";
    float dist = m_distanciaCm;
    float maxH = m_configManager->getMaxHeight();
    float minD = m_configManager->getMinDistance();
    float cap = m_configManager->getCapacity();
    
    s += "  Distancia medida: " + String(dist, 2) + " cm\n";
    s += "  maxHeight (fondo): " + String(maxH, 1) + " cm\n";
    s += "  minDistance (sensor): " + String(minD, 1) + " cm\n";
    s += "  capacidad: " + String(cap, 1) + " L\n";
    
    if (dist < 0) {
        s += "  ERROR: Lectura invalida\n";
    } else if (dist <= minD) {
        s += "  Resultado: LLENO (dist <= minDist)\n";
        s += "  Litros: " + String(cap, 1) + " L | Porcentaje: 100%";
    } else {
        float altLiq = maxH - dist;
        if (altLiq < 0) altLiq = 0;
        float altTotal = maxH - minD;
        float pct = (altTotal > 0) ? (altLiq / altTotal) * 100.0 : 0;
        if (pct > 100) pct = 100;
        if (pct < 0) pct = 0;
        float litros = (cap * pct) / 100.0;
        
        s += "  Altura liquido: " + String(maxH, 1) + " - " + String(dist, 2) + " = " + String(altLiq, 2) + " cm\n";
        s += "  Altura total util: " + String(maxH, 1) + " - " + String(minD, 1) + " = " + String(altTotal, 1) + " cm\n";
        s += "  Porcentaje: (" + String(altLiq, 2) + " / " + String(altTotal, 1) + ") * 100 = " + String(pct, 1) + "%\n";
        s += "  Litros: (" + String(cap, 1) + " * " + String(pct, 1) + "%) / 100 = " + String(litros, 1) + " L";
    }
    
    return s;
}

String LiquidLevelSensor::getName() const {
    return "LiquidLevel";
}

String LiquidLevelSensor::getJson() const {
    StaticJsonDocument<256> doc;
    doc["nombre"] = getName();
    doc["estado"] = m_mensajeError;
    if (m_distanciaCm >= 0) {
        doc["distancia_cm"] = serialized(String(m_distanciaCm, 1));
        doc["litros"] = serialized(calculateLitros());
    } else {
        doc["distancia_cm"] = "Error";
        doc["litros"] = "Error";
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

String LiquidLevelSensor::calculateLitros() const {
    if (m_distanciaCm < 0) { // Error de lectura del sensor
        return "Error";
    }
    return String(getLitros(), 1);
}

// Única fórmula de litros del sistema (dashboard, OLED, MQTT, historial, ESP-NOW).
// Antes había dos: capacidad*(maxH-d)/maxH y capacidad*porcentaje, y el OLED
// mostraba ~20L menos que Home Assistant con el tinaco casi lleno.
float LiquidLevelSensor::getLitros() const {
    if (m_distanciaCm < 0) return 0.0f;
    return m_configManager->getCapacity() * getPercentage() / 100.0f;
}

float LiquidLevelSensor::getPercentage() const {
    if (m_distanciaCm < 0) {
        return 0.0; // Error de lectura
    }
    
    // Si la distancia es menor o igual a la mínima del sensor, se considera lleno
    if (m_distanciaCm <= m_configManager->getMinDistance()) {
        return 100.0;
    }
    
    double alturaActualLiquido = m_configManager->getMaxHeight() - m_distanciaCm;
    double alturaTotalLiquido = m_configManager->getMaxHeight() - m_configManager->getMinDistance();
    
    if (alturaTotalLiquido <= 0) {
        return 0.0; // Error en configuración
    }
    
    double porcentajeLlenado = (alturaActualLiquido / alturaTotalLiquido) * 100.0;
    
    if (porcentajeLlenado > 100.0) porcentajeLlenado = 100.0;
    if (porcentajeLlenado < 0.0) porcentajeLlenado = 0.0;
    
    return porcentajeLlenado;
}