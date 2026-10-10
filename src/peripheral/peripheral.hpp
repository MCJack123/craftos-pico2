#ifndef PERIPHERAL_HPP
#define PERIPHERAL_HPP

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <craftos.h>
}

struct peripheral {
    const char * types[2];
    const luaL_Reg * funcs;
    int (*open)(bool right, void** userp_out);
    void (*close)(void* userp);
};

extern struct peripheral peripheral_drive;
extern struct peripheral peripheral_speaker;
extern struct peripheral peripheral_modem;

#endif