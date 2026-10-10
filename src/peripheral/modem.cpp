#include "peripheral.hpp"

static int modem_open(bool right, void** userp_out) {
    return -1;
}

static const luaL_Reg peripheral_methods[] = {
    {NULL, NULL}
};

struct peripheral peripheral_modem = {
    {"modem", NULL},
    peripheral_methods,
    modem_open,
    NULL
};
