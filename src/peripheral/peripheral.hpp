#ifndef PERIPHERAL_HPP
#define PERIPHERAL_HPP

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <craftos.h>
}

#define PERIPHERAL_CMD_GET_TYPE 1
#define PERIPHERAL_CMD_GET_PORT 2

struct peripheral {
    const char * types[3];
    const luaL_Reg * funcs;
    int (*open)(bool right, void** userp_out);
    void (*close)(void* userp);
};

extern struct peripheral peripheral_drive;
extern struct peripheral peripheral_speaker;
extern struct peripheral peripheral_modem;
extern int peripheral_detect_query(bool right, uint8_t cmd);

#endif