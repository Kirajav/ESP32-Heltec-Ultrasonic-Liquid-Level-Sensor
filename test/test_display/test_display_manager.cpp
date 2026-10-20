/**
 * Unit Tests - DisplayManager (Public API)
 * 
 * Testea la funcionalidad del display OLED Heltec WiFi Kit 32:
 * - Auto-sleep enable/disable
 * - Configuración de tiempo de auto-sleep
 * - Ciclo sleep/wake
 * - Control de LED
 * - Scroll horizontal
 * 
 * NOTA: Estos tests necesitan el ESP32 Heltec con display OLED conectado.
 * Usa solo la API pública de DisplayManager.
 * Se ejecutan con: pio test -e heltec_test -f test_display
 */

#include <unity.h>
#include <Arduino.h>
#include "DisplayManager.h"

DisplayManager* display = nullptr;

void setUp() {
    display = new DisplayManager();
    display->begin();
}

void tearDown() {
    delete display;
    display = nullptr;
}

// ============================================================
// Tests de Auto-Sleep configuración
// ============================================================

void test_set_auto_sleep_time() {
    display->setAutoSleepTime(60);
    TEST_ASSERT_EQUAL(60, display->getAutoSleepTime());
}

void test_set_auto_sleep_time_zero() {
    display->setAutoSleepTime(0);
    TEST_ASSERT_EQUAL(0, display->getAutoSleepTime());
}

void test_set_auto_sleep_time_large() {
    display->setAutoSleepTime(300);
    TEST_ASSERT_EQUAL(300, display->getAutoSleepTime());
}

// ============================================================
// Tests de Sleep/Wake
// ============================================================

void test_display_not_sleeping_initially() {
    TEST_ASSERT_FALSE(display->isDisplaySleeping());
}

void test_force_display_sleep() {
    display->forceDisplaySleep();
    TEST_ASSERT_TRUE(display->isDisplaySleeping());
}

void test_wake_up_from_sleep() {
    display->forceDisplaySleep();
    TEST_ASSERT_TRUE(display->isDisplaySleeping());
    
    display->wakeUpDisplay();
    TEST_ASSERT_FALSE(display->isDisplaySleeping());
}

void test_wake_up_when_not_sleeping() {
    // No debería causar problemas llamar wakeUp cuando no está dormido
    TEST_ASSERT_FALSE(display->isDisplaySleeping());
    display->wakeUpDisplay();
    TEST_ASSERT_FALSE(display->isDisplaySleeping());
}

void test_force_sleep_multiple_times() {
    display->forceDisplaySleep();
    display->forceDisplaySleep();
    TEST_ASSERT_TRUE(display->isDisplaySleeping());
    
    display->wakeUpDisplay();
    TEST_ASSERT_FALSE(display->isDisplaySleeping());
}

// ============================================================
// Tests de Auto-Sleep enable/disable
// ============================================================

void test_enable_auto_sleep() {
    display->enableAutoSleep(true);
    // No crashea - el display acepta el comando
    TEST_ASSERT_FALSE(display->isDisplaySleeping()); // Aún no debe dormir
}

void test_disable_auto_sleep() {
    display->enableAutoSleep(false);
    // No debe activar sleep incluso después de mucho tiempo
    display->checkAutoSleep();
    TEST_ASSERT_FALSE(display->isDisplaySleeping());
}

// ============================================================
// Tests de Display On/Off
// ============================================================

void test_display_on() {
    display->displayOn();
    // No crashea
    TEST_ASSERT_FALSE(display->isDisplaySleeping());
}

void test_display_off() {
    display->displayOff();
    // No crashea - esto apaga el display pero no es lo mismo que sleep
    TEST_ASSERT_TRUE(true); // El test es que no lance excepción
}

void test_handle_display_state_change_on() {
    display->handleDisplayStateChange(true);
    TEST_ASSERT_FALSE(display->isDisplaySleeping());
}

void test_handle_display_state_change_off() {
    display->handleDisplayStateChange(false);
    // No crashea
    TEST_ASSERT_TRUE(true);
}

// ============================================================
// Tests de LED control
// ============================================================

void test_led_state_on() {
    display->setLedState(true);
    // No crashea - el LED se enciende
    TEST_ASSERT_TRUE(true);
}

void test_led_state_off() {
    display->setLedState(false);
    // No crashea - el LED se apaga
    TEST_ASSERT_TRUE(true);
}

void test_start_fade_effect() {
    display->startFadeEffect();
    // No crashea
    TEST_ASSERT_TRUE(true);
}

void test_stop_fade_effect() {
    display->startFadeEffect();
    display->stopFadeEffect();
    // No crashea
    TEST_ASSERT_TRUE(true);
}

void test_update_fade_effect() {
    display->startFadeEffect();
    display->updateFadeEffect();
    // No crashea
    TEST_ASSERT_TRUE(true);
}

// ============================================================
// Tests de Scroll Horizontal
// ============================================================

void test_draw_scrolling_text_no_crash() {
    display->drawScrollingText(0, 0, "Texto largo para scroll horizontal", 64);
    // No crashea
    TEST_ASSERT_TRUE(true);
}

void test_update_scrolling_text_no_crash() {
    display->drawScrollingText(0, 0, "Texto largo para scroll", 64);
    display->updateScrollingText();
    // No crashea
    TEST_ASSERT_TRUE(true);
}

void test_draw_short_text_no_scroll() {
    display->drawScrollingText(0, 0, "Hola", 128);
    // Texto corto no debería causar problemas
    TEST_ASSERT_TRUE(true);
}

void test_draw_empty_text() {
    display->drawScrollingText(0, 0, "", 64);
    display->updateScrollingText();
    // No crashea con texto vacío
    TEST_ASSERT_TRUE(true);
}

// ============================================================
// Tests de Display content
// ============================================================

void test_clear_display() {
    display->clear();
    // No crashea
    TEST_ASSERT_TRUE(true);
}

void test_draw_string() {
    display->clear();
    display->drawString(0, 0, "Hello Test");
    display->display();
    // No crashea
    TEST_ASSERT_TRUE(true);
}

void test_draw_progress_bar() {
    display->clear();
    display->drawProgressBar(0, 50, 128, 10, 75);
    display->display();
    // No crashea
    TEST_ASSERT_TRUE(true);
}

void test_show_countdown() {
    display->showCountdown("Test countdown", 3);
    // No crashea
    TEST_ASSERT_TRUE(true);
}

void test_update_display_with_data() {
    display->updateDisplay("192.168.1.1", "45.2", "735.5");
    // No crashea - muestra datos del sensor
    TEST_ASSERT_TRUE(true);
}

void test_show_splash_screen() {
    display->showSplashScreen();
    // No crashea
    TEST_ASSERT_TRUE(true);
}

// ============================================================
// Tests de partial display update
// ============================================================

void test_fill_rect_black() {
    display->fillRect(0, 0, 128, 64, 0); // BLACK
    TEST_ASSERT_TRUE(true);
}

void test_fill_rect_white() {
    display->fillRect(0, 0, 64, 32, 1); // WHITE
    TEST_ASSERT_TRUE(true);
}

void test_set_color() {
    display->setColor(0); // BLACK
    display->setColor(1); // WHITE
    TEST_ASSERT_TRUE(true);
}

// ============================================================
// MAIN
// ============================================================

void setup() {
    delay(2000);
    
    UNITY_BEGIN();
    
    // Auto-Sleep config
    RUN_TEST(test_set_auto_sleep_time);
    RUN_TEST(test_set_auto_sleep_time_zero);
    RUN_TEST(test_set_auto_sleep_time_large);
    
    // Sleep/Wake
    RUN_TEST(test_display_not_sleeping_initially);
    RUN_TEST(test_force_display_sleep);
    RUN_TEST(test_wake_up_from_sleep);
    RUN_TEST(test_wake_up_when_not_sleeping);
    RUN_TEST(test_force_sleep_multiple_times);
    
    // Auto-sleep enable/disable
    RUN_TEST(test_enable_auto_sleep);
    RUN_TEST(test_disable_auto_sleep);
    
    // Display On/Off
    RUN_TEST(test_display_on);
    RUN_TEST(test_display_off);
    RUN_TEST(test_handle_display_state_change_on);
    RUN_TEST(test_handle_display_state_change_off);
    
    // LED
    RUN_TEST(test_led_state_on);
    RUN_TEST(test_led_state_off);
    RUN_TEST(test_start_fade_effect);
    RUN_TEST(test_stop_fade_effect);
    RUN_TEST(test_update_fade_effect);
    
    // Scroll
    RUN_TEST(test_draw_scrolling_text_no_crash);
    RUN_TEST(test_update_scrolling_text_no_crash);
    RUN_TEST(test_draw_short_text_no_scroll);
    RUN_TEST(test_draw_empty_text);
    
    // Display content
    RUN_TEST(test_clear_display);
    RUN_TEST(test_draw_string);
    RUN_TEST(test_draw_progress_bar);
    RUN_TEST(test_show_countdown);
    RUN_TEST(test_update_display_with_data);
    RUN_TEST(test_show_splash_screen);
    
    // Partial update
    RUN_TEST(test_fill_rect_black);
    RUN_TEST(test_fill_rect_white);
    RUN_TEST(test_set_color);
    
    UNITY_END();
}

void loop() {}
