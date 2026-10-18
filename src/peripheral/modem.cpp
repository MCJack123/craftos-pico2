#include <cyw43_config_craftos.h>
#include "peripheral.hpp"
#include <pico/cyw43_driver.h>
#include <pico/cyw43_arch.h>
#include <pico/async_context.h>
#include <pico/async_context_freertos.h>

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
    if (status == CYW43_LINK_UP) {
        lua_createtable(L, 0, 4);
        // lua_pushstring(L, CYW43_IOCTL_GET_SSID);
        // lua_setfield(L, -2, "network");
        // lua_pushnumber(L, status.bars);
        // lua_setfield(L, -2, "bars");
        // lua_pushliteral(L, "802.11");
        // if (status.mode == 'x') lua_pushliteral(L, "ax");
        // else lua_pushlstring(L, &status.mode, 1);
        // lua_concat(L, 2);
        // lua_setfield(L, -2, "mode");
        // lua_pushstring(L, wifi_security_names[status.security]);
        // lua_setfield(L, -2, "security");
    } else lua_pushnil(L);
    return 1;
}

static int wifi_scan(lua_State *L) {
    lua_remove(L, 1);
    return 0;
}

static int wifi_connect(lua_State *L) {
    lua_remove(L, 1);
    return 0;
}

static int wifi_disconnect(lua_State *L) {
    lua_remove(L, 1);
    return 0;
}

static bool isModemInit = false;
static uint cyw_pins_left[CYW43_PIN_INDEX_WL_COUNT] = {8, 11, 11, 11, 10, 9},
    cyw_pins_right[CYW43_PIN_INDEX_WL_COUNT] = {32, 35, 35, 35, 34, 33};
static int modem_open(bool right, void** userp_out) {
    if (isModemInit) return -1; // cannot use two modems
    cyw43_set_pins_wl(right ? cyw_pins_right : cyw_pins_left);
    if (cyw43_arch_init() != 0) return -2;
    isModemInit = true;
    *userp_out = (void*)1;
    return 0;
}

static void modem_close(void* userp) {
    cyw43_arch_deinit();
    isModemInit = false;
}

static const luaL_Reg peripheral_methods[] = {
    {NULL, NULL}
};

struct peripheral peripheral_modem = {
    {"modem", "wifi", NULL},
    peripheral_methods,
    modem_open,
    modem_close
};
