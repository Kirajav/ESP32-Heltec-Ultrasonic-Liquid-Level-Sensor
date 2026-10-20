/*
 * ESP32 Download Mode Controller
 * Para Arduino Mega 2560
 * 
 * Este código permite controlar automáticamente el ESP32
 * para ponerlo en modo download sin tocar botones físicos.
 * 
 * CONEXIONES:
 * Arduino Pin 2  →  ESP32 GPIO0 (BOOT)
 * Arduino Pin 3  →  ESP32 EN (RESET)
 * Arduino Pin 13 →  LED integrado (indicador)
 * Arduino GND    →  ESP32 GND
 * 
 * COMANDOS SERIE:
 * - "DOWNLOAD" = Poner ESP32 en modo download
 * - "RELEASE"  = Liberar ESP32 a modo normal
 * - "RESET"    = Reset simple del ESP32
 * - "STATUS"   = Ver estado actual
 * - "HELP"     = Mostrar ayuda
 */

// Definir pines de control
#define ESP32_BOOT_PIN 2     // Pin para controlar GPIO0 (BOOT) del ESP32
#define ESP32_EN_PIN 3       // Pin para controlar EN (RESET) del ESP32
#define LED_PIN 13           // LED integrado del Arduino Mega

// Variables de estado
bool downloadMode = false;
unsigned long lastCommand = 0;

void setup() {
  // Inicializar comunicación serie
  Serial.begin(9600);
  while (!Serial) {
    ; // Esperar a que se conecte el puerto serie
  }
  
  // Configurar pines como salidas
  pinMode(ESP32_BOOT_PIN, OUTPUT);
  pinMode(ESP32_EN_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  
  // Estado inicial: ESP32 en modo normal
  setNormalMode();
  
  // Mensaje de bienvenida
  Serial.println("=========================================");
  Serial.println("ESP32 Download Mode Controller v1.0");
  Serial.println("Arduino Mega 2560 - Por GitHub Copilot");
  Serial.println("=========================================");
  Serial.println("Comandos disponibles:");
  Serial.println("- DOWNLOAD: Modo download ESP32");
  Serial.println("- RELEASE:  Modo normal ESP32");
  Serial.println("- RESET:    Reset simple ESP32");
  Serial.println("- STATUS:   Ver estado actual");
  Serial.println("- HELP:     Mostrar esta ayuda");
  Serial.println("=========================================");
  Serial.println("✅ Arduino listo. ESP32 en modo normal.");
  Serial.println();
}

void loop() {
  // Leer comandos del puerto serie
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toUpperCase();
    
    lastCommand = millis();
    processCommand(command);
  }
  
  // Parpadear LED si está en modo download
  if (downloadMode) {
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 500) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      lastBlink = millis();
    }
  }
  
  // Auto-release después de 5 minutos en modo download
  if (downloadMode && (millis() - lastCommand > 300000)) {
    Serial.println("⚠️  Auto-release después de 5 minutos");
    setNormalMode();
  }
}

void processCommand(String command) {
  Serial.println("📡 Comando recibido: " + command);
  
  if (command == "DOWNLOAD") {
    enterDownloadMode();
  } 
  else if (command == "RELEASE") {
    setNormalMode();
  }
  else if (command == "RESET") {
    resetESP32();
  }
  else if (command == "STATUS") {
    showStatus();
  }
  else if (command == "HELP") {
    showHelp();
  }
  else {
    Serial.println("❌ Comando no reconocido: " + command);
    Serial.println("💡 Escribe 'HELP' para ver comandos disponibles");
  }
  
  Serial.println();
}

void enterDownloadMode() {
  Serial.println("🔄 Poniendo ESP32 en modo DOWNLOAD...");
  
  downloadMode = true;
  digitalWrite(LED_PIN, HIGH);
  
  // Secuencia para modo download:
  // 1. BOOT = LOW (presionar)
  // 2. EN = LOW (reset)  
  // 3. EN = HIGH (liberar reset)
  // 4. BOOT permanece LOW
  
  digitalWrite(ESP32_BOOT_PIN, LOW);   // Presionar BOOT
  delay(100);
  digitalWrite(ESP32_EN_PIN, LOW);     // Reset
  delay(200);
  digitalWrite(ESP32_EN_PIN, HIGH);    // Liberar reset
  delay(100);
  
  Serial.println("✅ ESP32 en modo DOWNLOAD");
  Serial.println("📥 Listo para recibir firmware:");
  Serial.println("   pio run -t upload");
  Serial.println("⏰ Auto-release en 5 minutos");
}

void setNormalMode() {
  Serial.println("🔄 Poniendo ESP32 en modo NORMAL...");
  
  downloadMode = false;
  digitalWrite(LED_PIN, LOW);
  
  // Liberar ambos pines y hacer reset
  digitalWrite(ESP32_BOOT_PIN, HIGH);  // Liberar BOOT
  delay(50);
  digitalWrite(ESP32_EN_PIN, LOW);     // Reset
  delay(200);
  digitalWrite(ESP32_EN_PIN, HIGH);    // Liberar reset
  delay(100);
  
  Serial.println("✅ ESP32 en modo NORMAL");
  Serial.println("🚀 ESP32 ejecutando firmware");
}

void resetESP32() {
  Serial.println("🔄 Haciendo RESET del ESP32...");
  
  // Reset simple sin cambiar modo
  digitalWrite(ESP32_EN_PIN, LOW);
  delay(200);
  digitalWrite(ESP32_EN_PIN, HIGH);
  delay(100);
  
  Serial.println("✅ ESP32 reiniciado");
}

void showStatus() {
  Serial.println("📊 ESTADO ACTUAL:");
  Serial.println("- Modo ESP32: " + String(downloadMode ? "DOWNLOAD" : "NORMAL"));
  Serial.println("- Pin BOOT (GPIO0): " + String(digitalRead(ESP32_BOOT_PIN) ? "HIGH" : "LOW"));
  Serial.println("- Pin EN (RESET): " + String(digitalRead(ESP32_EN_PIN) ? "HIGH" : "LOW"));
  Serial.println("- LED indicador: " + String(digitalRead(LED_PIN) ? "ON" : "OFF"));
  Serial.println("- Tiempo activo: " + String(millis() / 1000) + " segundos");
}

void showHelp() {
  Serial.println("📖 AYUDA - Comandos disponibles:");
  Serial.println();
  Serial.println("DOWNLOAD - Poner ESP32 en modo download");
  Serial.println("  • GPIO0 = LOW, Reset automático");
  Serial.println("  • LED parpadea indicando modo activo");
  Serial.println("  • Listo para: pio run -t upload");
  Serial.println();
  Serial.println("RELEASE - Liberar ESP32 a modo normal");
  Serial.println("  • GPIO0 = HIGH, Reset automático");
  Serial.println("  • LED apagado");
  Serial.println("  • ESP32 ejecuta firmware normalmente");
  Serial.println();
  Serial.println("RESET - Reset simple del ESP32");
  Serial.println("  • Pulso en pin EN");
  Serial.println("  • No cambia el modo actual");
  Serial.println();
  Serial.println("STATUS - Mostrar estado actual");
  Serial.println("  • Estado de pines y modo");
  Serial.println();
  Serial.println("HELP - Mostrar esta ayuda");
  Serial.println();
  Serial.println("🔧 CONEXIONES NECESARIAS:");
  Serial.println("Arduino Pin 2  →  ESP32 GPIO0 (BOOT)");
  Serial.println("Arduino Pin 3  →  ESP32 EN (RESET)");
  Serial.println("Arduino GND    →  ESP32 GND");
}