#ifndef WIFI_DEBUG_FIX_H
#define WIFI_DEBUG_FIX_H

// Fix para la macro ESP_WM_MARK que no está definida en ESPAsync_WiFiManager
#ifndef ESP_WM_MARK
#define ESP_WM_MARK "[WM]"
#endif

#endif // WIFI_DEBUG_FIX_H