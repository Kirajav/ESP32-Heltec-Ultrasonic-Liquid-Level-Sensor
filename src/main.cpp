/**
 * ================================================================
 * MAIN.CPP - Entry Point del Sistema ESP32 Water Level Sensor
 * ================================================================
 * 
 * 📋 PROPÓSITO:
 * Este archivo contiene el punto de entrada principal del firmware
 * ESP32 para el sistema de monitoreo de nivel de agua con capacidades
 * avanzadas de conectividad y gestión web.
 * 
 * 🎯 RESPONSABILIDADES PRINCIPALES:
 * • Inicialización del hardware ESP32 y periféricos
 * • Setup de todas las librerías y managers del sistema
 * • Ejecución del loop principal de tareas
 * • Gestión del ciclo de vida de la aplicación
 * • Manejo de excepciones críticas y recovery
 * 
 * 🏗️ ARQUITECTURA:
 * Utiliza patrón Singleton via AppManager para centralizar
 * toda la lógica del sistema y garantizar una inicialización
 * ordenada y controlada de todos los componentes.
 * 
 * ⚡ OPTIMIZACIONES:
 * • Inicialización rápida con prioridades definidas
 * • Loop optimizado para bajo consumo energético
 * • Gestión eficiente de memoria y recursos
 * 
 * 🔧 DEBUGGING:
 * Habilitar CORE_DEBUG_LEVEL=3 en platformio.ini para
 * obtener información detallada de debugging en Serial Monitor.
 * 
 * @author: Sistema IoT ESP32 Team
 * @version: 3.4.2
 * @date: Octubre 2026
 * @hardware: ESP32 Heltec WiFi Kit 32 (con OLED integrado)
 * @framework: Arduino Core para ESP32 v2.0+
 */

#include "AppManager.h"          // ← Gestor principal del sistema (patrón Singleton)

/**
 * 🏠 INSTANCIA GLOBAL DEL GESTOR DE APLICACIÓN
 * ============================================
 * 
 * Instancia única del AppManager que coordina todos los
 * componentes del sistema. Utiliza patrón Singleton para
 * garantizar una sola instancia y acceso global controlado.
 * 
 * El AppManager encapsula:
 * • Configuración del sistema
 * • Gestión de conectividad (WiFi/ESP-NOW)
 * • Servidor web y APIs
 * • Manejo de sensores y actuadores
 * • Display OLED y interfaces de usuario
 * • OTA updates y mantenimiento
 */
AppManager app;

/**
 * 🚀 FUNCIÓN SETUP - INICIALIZACIÓN DEL SISTEMA
 * ============================================
 * 
 * Se ejecuta UNA SOLA VEZ al arrancar o reiniciar el ESP32.
 * Inicializa todos los componentes del sistema en el orden
 * correcto para evitar conflictos de dependencias.
 * 
 * 📋 SECUENCIA DE INICIALIZACIÓN:
 * 1. Comunicación serie para debugging
 * 2. AppManager.initialize() que gestiona:
 *    • SPIFFS file system para archivos web
 *    • Configuración desde memoria NVRAM
 *    • Hardware (sensores, display, LEDs)
 *    • Conectividad (WiFi, ESP-NOW)
 *    • Servidor web y rutas API
 *    • OTA updater y security
 *    • Task scheduler y timers
 * 
 * ⚠️ NOTA IMPORTANTE:
 * Si la inicialización falla, el sistema entrará en modo
 * de recuperación seguro con funcionalidad limitada pero
 * operativa para diagnóstico y reparación remota.
 * 
 * 🔧 CONFIGURACIÓN SERIAL:
 * • Velocidad: 115200 bps (estándar industrial)
 * • Compatible con Arduino IDE Serial Monitor
 * • Compatible con PlatformIO Serial Monitor
 * • Filtros ESP32 exception decoder habilitados
 */
void setup() {
    // ═══════════════════════════════════════════════════════════════
    // 📡 INICIALIZAR COMUNICACIÓN SERIE
    // ═══════════════════════════════════════════════════════════════
    Serial.begin(115200);
    
    // Esperar estabilización del puerto serie (especialmente en USB CDC)
    delay(100);
    
    // Banner de inicio con información del sistema
    Serial.println();
    Serial.println("████████████████████████████████████████████████████████");
    Serial.println("█                                                      █");
    Serial.println("█       ESP32 LIQUID LEVEL SENSOR SYSTEM v3.4.2       █");
    Serial.println("█              Enterprise Architecture                 █");
    Serial.println("█                                                      █");
    Serial.println("████████████████████████████████████████████████████████");
    Serial.println();
    Serial.println("🚀 INICIANDO SISTEMA COMPLETO...");
    Serial.println("📅 Build Date: " __DATE__ " " __TIME__);
    Serial.println("🔧 Hardware: ESP32 Heltec WiFi Kit 32");
    Serial.println("📦 Framework: Arduino Core + PlatformIO");
    Serial.println("🌐 Conectividad: WiFi + ESP-NOW Mesh");
    Serial.println("💻 Web Server: AsyncWebServer + SPIFFS");
    Serial.println();
    
    // ═══════════════════════════════════════════════════════════════
    // 🏗️ INICIALIZAR GESTOR PRINCIPAL DE APLICACIÓN
    // ═══════════════════════════════════════════════════════════════
    app.initialize();
    
    // Mensaje de confirmación de inicialización exitosa
    String deviceIP = app.getDeviceIP();
    Serial.println();
    Serial.println("✅ ¡SISTEMA INICIALIZADO CORRECTAMENTE!");
    Serial.println("🌐 Dashboard disponible en: http://" + deviceIP + "/");
    Serial.println("🔧 Actualización OTA: http://" + deviceIP + "/update");
    Serial.println("📡 WebSerial debug: http://" + deviceIP + "/webserial");
    Serial.println("ℹ️  Portal cautivo: solo activo en modo AP (doble reset)");
    Serial.println();
    Serial.println("════════════════════════════════════════════════════════");
}

/**
 * 🔄 FUNCIÓN LOOP - CICLO PRINCIPAL DE EJECUCIÓN
 * =============================================
 * 
 * Se ejecuta CONTINUAMENTE mientras el ESP32 esté encendido.
 * Gestiona todas las tareas recurrentes del sistema de forma
 * eficiente y optimizada para bajo consumo energético.
 * 
 * 📋 TAREAS EJECUTADAS EN CADA CICLO:
 * • Manejo de peticiones del servidor web
 * • Procesamiento de actualizaciones OTA
 * • Lectura y procesamiento de sensores
 * • Gestión de la red mesh ESP-NOW
 * • Actualización del display OLED
 * • Mantenimiento de conectividad WiFi
 * • Gestión de memoria y cleanup
 * • Watchdog timer feeding
 * 
 * ⚡ OPTIMIZACIONES DE RENDIMIENTO:
 * • Task scheduling inteligente por prioridades
 * • Yield() calls para evitar watchdog resets
 * • Memory pool management eficiente
 * • CPU throttling automático en idle
 * • Sleep modes cuando es apropiado
 * 
 * 🔧 MONITOREO Y DEBUGGING:
 * El loop principal incluye monitoreo de performance
 * y métricas de sistema que se pueden consultar via
 * endpoints de API especializados.
 * 
 * 📊 MÉTRICAS MONITOREADAS:
 * • Tiempo de ejecución por ciclo (microsegundos)
 * • Uso de memoria RAM (heap disponible)
 * • Uso de CPU (% de tiempo activo)
 * • Número de tareas ejecutadas por segundo
 * • Latencia de respuesta de red
 * • Estado de conectividad WiFi/ESP-NOW
 * 
 * ⚠️ MANEJO DE ERRORES:
 * Si alguna tarea crítica falla, el sistema puede:
 * • Reintentar la operación con backoff exponencial
 * • Entrar en modo seguro con funcionalidad reducida
 * • Reiniciar componentes específicos sin reset completo
 * • Reportar errores via log y APIs de monitoreo
 */
void loop() {
    // ═══════════════════════════════════════════════════════════════
    // 🎯 EJECUTAR TODAS LAS TAREAS DEL SISTEMA
    // ═══════════════════════════════════════════════════════════════
    app.loop();
    
    // ═══════════════════════════════════════════════════════════════
    // ⚡ CEDER CPU AL IDLE TASK (REDUCE CALENTAMIENTO)
    // ═══════════════════════════════════════════════════════════════
    // yield() solo cede a tareas de igual o mayor prioridad: el idle
    // task (prioridad 0) nunca corría en el Core 1 y la CPU giraba al
    // 100% sin pausa. Con delay(10) el Core 1 duerme (WAITI) entre
    // iteraciones; 100 Hz sobra para DNS del portal, botón PRG y WebSerial.
    delay(10);
}
