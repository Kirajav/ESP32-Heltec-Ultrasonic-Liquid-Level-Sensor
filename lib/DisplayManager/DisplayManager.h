#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <heltec.h>

class DisplayManager {
public:
    DisplayManager();
    void begin();
    void showCountdown(const char* message, int seconds);
    void handleDisplayStateChange(bool turnOn);
    void updateDisplay(const String& ip, const String& dist, const String& litros);
    void clear();
    void display();
    void setTextAlignment(int alignment);
    void setFont(const uint8_t* font);
    void drawString(int x, int y, const String& text);
    void displayOn();
    void displayOff();
    void drawProgressBar(int x, int y, int width, int height, int progress);
    
    // Auto-sleep control
    void setAutoSleepTime(uint16_t seconds);
    uint16_t getAutoSleepTime() const;
    void enableAutoSleep(bool enable);  // Solo se activa en modo sensor
    bool isAutoSleepEnabled() const;
    void checkAutoSleep();
    void wakeUpDisplay();  // Solo responde a PRG o botón web
    void forceDisplaySleep();
    bool isDisplaySleeping() const;
    void showSplashScreen();
    void handlePRGButton();  // Manejo del botón PRG
    
    // Control del LED blanco
    void setLedState(bool state);
    void startFadeEffect();
    void stopFadeEffect();
    void updateFadeEffect();
    
    // Scroll horizontal para textos largos
    void drawScrollingText(int x, int y, const String& text, int maxWidth);
    void updateScrollingText();  // Llamar en loop() para animar
    
    // Métodos para actualización parcial del display
    void fillRect(int x, int y, int width, int height, int color);  // color: 0=BLACK, 1=WHITE
    void setColor(int color);  // 0=BLACK, 1=WHITE

private:
    void showInitialMessage();
    
    // Variables para el LED
    bool _ledFadeActive;
    unsigned long _lastFadeUpdate;
    int _fadeDirection;
    int _fadeValue;
    static const int LED_PIN = 25;
    static const int LED_CHANNEL = 0;
    static const int LED_FREQ = 5000;
    static const int LED_RESOLUTION = 8;
    
    // Auto-sleep variables
    uint16_t _autoSleepTime;        // tiempo en segundos
    unsigned long _lastActivity;    // última actividad detectada
    bool _displaySleeping;          // estado de la pantalla
    bool _autoSleepEnabled;         // habilitado solo en modo sensor
    bool _countdownActive;          // mostrando countdown de apagado
    int _countdownSeconds;          // segundos restantes del countdown
    unsigned long _countdownStart;  // inicio del countdown
    
    // Pin del botón PRG
    static const int PRG_BUTTON_PIN = 0;
    bool _lastButtonState;
    unsigned long _lastButtonPress;
    
    // Scroll horizontal variables
    String _scrollText;
    int _scrollX;
    int _scrollY;
    int _scrollMaxWidth;
    int _scrollOffset;
    unsigned long _lastScrollUpdate;
    static const int SCROLL_SPEED_MS = 150;  // ms entre movimientos
};

#endif // DISPLAY_MANAGER_H