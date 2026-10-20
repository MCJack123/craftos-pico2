#include <cyw43_config_craftos.h>
#include "peripheral.hpp"
#include <FreeRTOS.h>
#include <timers.h>
#include <pico/cyw43_driver.h>
#include <pico/cyw43_arch.h>
#include <pico/async_context.h>
#include <pico/async_context_freertos.h>
#include "../event.hpp"

static int modem_open(lua_State *L) {
    lua_remove(L, 1);
    return 0;
}

static int modem_isOpen(lua_State *L) {
    lua_remove(L, 1);
    return 0;
}

static int modem_close(lua_State *L) {
    lua_remove(L, 1);
    return 0;
}

static int modem_closeAll(lua_State *L) {

    return 0;
}

static int modem_transmit(lua_State *L) {
    lua_remove(L, 1);
    return 0;
}

static int modem_isWireless(lua_State *L) {
    lua_pushboolean(L, true);
    return 1;
}

static int wifi_status(lua_State *L) {
    int status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    if (status == CYW43_LINK_JOIN) {
        lua_createtable(L, 0, 4);
        uint8_t bssid[6];
        cyw43_wifi_get_bssid(&cyw43_state, bssid);
        lua_pushfstring(L, "%02x:%02x:%02x:%02x:%02x:%02x", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
        lua_setfield(L, -2, "network");
        int32_t bars;
        cyw43_wifi_get_rssi(&cyw43_state, &bars);
        bars = (bars + 90) / 10;
        if (bars > 4) bars = 4;
        lua_pushinteger(L, bars);
        lua_setfield(L, -2, "bars");
        // TODO
        lua_pushliteral(L, "unknown");
        lua_setfield(L, -2, "mode");
        lua_pushliteral(L, "unknown");
        lua_setfield(L, -2, "security");
    } else lua_pushnil(L);
    return 1;
}

static int scan_results(void* userp, const cyw43_ev_scan_result_t * network) {
    event_t * event = (event_t*)userp;
    if (event->wifi.networkCount == 0) event->wifi.networks = (wifi_network_t*)malloc(sizeof(wifi_network_t));
    else event->wifi.networks = (wifi_network_t*)realloc(event->wifi.networks, sizeof(wifi_network_t) * (event->wifi.networkCount + 1));
    memset(event->wifi.networks[event->wifi.networkCount].ssid, 0, 33);
    strncpy(event->wifi.networks[event->wifi.networkCount].ssid, (const char*)network->ssid, network->ssid_len);
    event->wifi.networks[event->wifi.networkCount].security = network->auth_mode;
    int bars = (network->rssi + 90) / 10;
    if (bars > 4) bars = 4;
    event->wifi.networks[event->wifi.networkCount++].bars = bars;
    return 0;
}

static void scan_complete(TimerHandle_t timer) {
    event_t * event = (event_t*)pvTimerGetTimerID(timer);
    event_push(event);
    xTimerDelete(timer, 0);
}

static event_t wifi_event;

static int wifi_scan(lua_State *L) {
    lua_remove(L, 1);
    if (cyw43_wifi_scan_active(&cyw43_state)) {
        lua_pushboolean(L, false);
        return 1;
    }
    cyw43_wifi_scan_options_t options;
    memset(&options, 0, sizeof(cyw43_wifi_scan_options_t));
    if (wifi_event.type == EVENT_TYPE_WIFI_SCAN && wifi_event.wifi.networks != NULL) free(wifi_event.wifi.networks);
    wifi_event.type = EVENT_TYPE_WIFI_SCAN;
    wifi_event.wifi.networkCount = 0;
    wifi_event.wifi.networks = NULL;
    int res = cyw43_wifi_scan(&cyw43_state, &options, &wifi_event, scan_results);
    if (res == 0) xTimerStart(xTimerCreate("wifi scan event", pdMS_TO_TICKS(10000), pdFALSE, &wifi_event, scan_complete), portMAX_DELAY);
    lua_pushboolean(L, res == 0);
    return 1;
}

static const int auth_modes[] = {
    CYW43_AUTH_OPEN,
    -1,
    CYW43_AUTH_WPA_TKIP_PSK,
    CYW43_AUTH_WPA_TKIP_PSK,
    CYW43_AUTH_WPA2_AES_PSK,
    CYW43_AUTH_WPA2_AES_PSK,
    CYW43_AUTH_WPA2_MIXED_PSK,
    CYW43_AUTH_WPA2_MIXED_PSK,
    CYW43_AUTH_WPA3_SAE_AES_PSK,
    CYW43_AUTH_WPA3_SAE_AES_PSK,
    CYW43_AUTH_WPA3_SAE_AES_PSK,
    CYW43_AUTH_WPA3_SAE_AES_PSK,
    CYW43_AUTH_WPA3_WPA2_AES_PSK,
    CYW43_AUTH_WPA3_WPA2_AES_PSK,
    CYW43_AUTH_WPA3_WPA2_AES_PSK,
    CYW43_AUTH_WPA3_WPA2_AES_PSK,
};

static struct {
    char* ssid;
    char* password;
} wifi_connect_state = {NULL, NULL};

static int connect_results(void* userp, const cyw43_ev_scan_result_t * network) {
    if (wifi_connect_state.ssid == NULL) return 0;
    if (strncmp((const char*)network->ssid, wifi_connect_state.ssid, network->ssid_len <= strlen(wifi_connect_state.ssid) ? network->ssid_len : strlen(wifi_connect_state.ssid))) {
        int ok = cyw43_arch_wifi_connect_async(wifi_connect_state.ssid, wifi_connect_state.password, auth_modes[network->auth_mode]);
        free(wifi_connect_state.ssid);
        wifi_connect_state.ssid = NULL;
        if (wifi_connect_state.password) {
            free(wifi_connect_state.password);
            wifi_connect_state.password = NULL;
        }
        if (ok != 0) {
            event_t event;
            event.type = EVENT_TYPE_WIFI_DISCONNECT;
            event_push(&event);
        }
    }
    return 0;
}

static int wifi_connect(lua_State *L) {
    lua_remove(L, 1);
    const char * ssid = luaL_checkstring(L, 1);
    const char * password = luaL_optstring(L, 2, NULL);
    // painful way to find auth type
    int ok;
    if (password == NULL) ok = cyw43_arch_wifi_connect_async(ssid, password, CYW43_AUTH_OPEN);
    else {
        int auth_type = -1;
        if (wifi_event.type == EVENT_TYPE_WIFI_SCAN) {
            for (int i = 0; i < wifi_event.wifi.networkCount; i++) {
                if (strncmp(wifi_event.wifi.networks[i].ssid, ssid, 32) == 0) {
                    auth_type = auth_modes[wifi_event.wifi.networks[i].security];
                    break;
                }
            }
        }
        if (auth_type != -1) ok = cyw43_arch_wifi_connect_async(ssid, password, auth_type);
        else {
            // ugh we've gotta scan before connecting
            // at least we can filter and continue async
            cyw43_wifi_scan_options_t options;
            memset(&options, 0, sizeof(cyw43_wifi_scan_options_t));
            options.ssid_len = strlen(ssid);
            if (options.ssid_len > 32) options.ssid_len = 32;
            memcpy(options.ssid, ssid, options.ssid_len);
            if (wifi_connect_state.ssid != NULL) free(wifi_connect_state.ssid);
            wifi_connect_state.ssid = strdup(ssid);
            if (wifi_connect_state.password != NULL) free(wifi_connect_state.password);
            wifi_connect_state.password = password ? strdup(password) : NULL;
            ok = cyw43_wifi_scan(&cyw43_state, &options, &wifi_connect_state, connect_results);
        }
    }
    lua_pushboolean(L, ok == 0);
    return 1;
}

static int wifi_disconnect(lua_State *L) {
    lua_remove(L, 1);
    if (cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_JOIN) 
        lua_pushboolean(L, cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA) == 0);
    else lua_pushboolean(L, false);
    return 1;
}

static int lastWifiStatus = CYW43_LINK_DOWN;
static void wifi_watch(TimerHandle_t timer) {
    int status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (status != lastWifiStatus) {
        event_t event;
        event.type = EVENT_TYPE_NONE;
        switch (status) {
            case CYW43_LINK_DOWN: if (lastWifiStatus != CYW43_LINK_UP) break; // fallthrough
            case CYW43_LINK_BADAUTH:
            case CYW43_LINK_FAIL:
            case CYW43_LINK_NONET: event.type = EVENT_TYPE_WIFI_DISCONNECT; break;
            case CYW43_LINK_UP: event.type = EVENT_TYPE_WIFI_CONNECT; break;
        }
        if (event.type != EVENT_TYPE_NONE) event_push(&event);
    }
    lastWifiStatus = status;
}

static bool isModemInit = false;
static TimerHandle_t watch_timer;
static uint cyw_pins_left[CYW43_PIN_INDEX_WL_COUNT] = {8, 11, 11, 11, 10, 9},
    cyw_pins_right[CYW43_PIN_INDEX_WL_COUNT] = {32, 35, 35, 35, 34, 33};
static int modem_open(bool right, void** userp_out) {
    if (isModemInit) return -1; // cannot use two modems
    cyw43_set_pins_wl(right ? cyw_pins_right : cyw_pins_left);
    int tries = 0;
init:
    if (cyw43_arch_init() != 0) return -2;
    cyw43_arch_enable_sta_mode();
    if (!cyw43_state.itf_state) {
        cyw43_arch_deinit();
        if (tries++ > 20) return -3;
        goto init;
    }
    lastWifiStatus = CYW43_LINK_DOWN;
    watch_timer = xTimerCreate("wifi watch", pdMS_TO_TICKS(1000), pdTRUE, NULL, wifi_watch);
    xTimerStart(watch_timer, portMAX_DELAY);
    isModemInit = true;
    *userp_out = (void*)1;
    return 0;
}

static void modem_close(void* userp) {
    xTimerStop(watch_timer, portMAX_DELAY);
    xTimerDelete(watch_timer, portMAX_DELAY);
    cyw43_arch_deinit();
    isModemInit = false;
}

static const luaL_Reg peripheral_methods[] = {
    {"scan", wifi_scan},
    {"status", wifi_status},
    {"connect", wifi_connect},
    {"disconnect", wifi_disconnect},
    {NULL, NULL}
};

struct peripheral peripheral_modem = {
    {"modem", "wifi", NULL},
    peripheral_methods,
    modem_open,
    modem_close
};
