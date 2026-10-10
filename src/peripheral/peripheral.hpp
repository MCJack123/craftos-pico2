#ifndef PERIPHERAL_HPP
#define PERIPHERAL_HPP

#include <FreeRTOS.h>
#include <semphr.h>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <craftos.h>
}
#include "../drivers/1wire.h"

#define PERIPHERAL_CMD_GET_TYPE 1
#define PERIPHERAL_CMD_GET_PORT 2

struct peripheral {
    const char * types[2];
    const luaL_Reg * funcs;
    int (*open)(bool right, void** userp_out);
    void (*close)(void* userp);
};

extern struct peripheral peripheral_drive;
extern struct peripheral peripheral_speaker;
extern struct peripheral peripheral_modem;
extern SemaphoreHandle_t onewire_lock[2];
extern OW onewire_inst[2];

#endif