#include <string.h>
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "modules/mmfs.h"
#include <sfe_psram.h>
}
#include <FreeRTOS.h>
#include <task.h>
#include "event.hpp"
#include "modules/terminal.hpp"
#include <fcntl.h>
#include <hardware/flash.h>
#include <sfe_pico_alloc.h>

extern "C" struct mmfs_mount* rom;

static const char* eventNames[] = {
    "",
    "key",
    "key_up",
    "char",
    "timer",
    "alarm",
    "disk",
    "disk_eject",
    "paste",
    "redstone",
    "speaker_audio_empty",
    "terminate",
};

extern "C" {
    extern const luaL_Reg fs_lib[];
    extern const luaL_Reg os_lib[];
    extern const luaL_Reg peripheral_lib[];
    extern const luaL_Reg rs_lib[];
    extern const luaL_Reg term_lib[];
}

lua_State *paramQueue;

static int getNextEvent(lua_State *L, const char * filter) {
    lua_State *event = NULL;
    do {
        if (event) lua_remove(paramQueue, 1);
        while (lua_gettop(paramQueue) == 0) {
            event_t ev;
            event_wait(&ev);
            event = lua_newthread(paramQueue);
            lua_pushstring(event, eventNames[ev.type]);
            switch (ev.type) {
                case EVENT_TYPE_KEY: case EVENT_TYPE_KEY_UP:
                    lua_pushinteger(event, ev.key.keycode);
                    if (ev.type == EVENT_TYPE_KEY) lua_pushboolean(event, ev.key.repeat);
                    break;
                case EVENT_TYPE_CHAR:
                    lua_pushlstring(event, &ev.character.c, 1);
                    break;
                case EVENT_TYPE_TIMER: case EVENT_TYPE_ALARM:
                    lua_pushinteger(event, ev.timer.timerID);
                    break;
                case EVENT_TYPE_DISK: case EVENT_TYPE_DISK_EJECT:
                    lua_pushliteral(event, "right");
                    break;
                case EVENT_TYPE_SPEAKER_AUDIO_EMPTY:
                    lua_pushliteral(event, "left");
                    break;
            }
        }
        event = lua_tothread(paramQueue, 1);
    } while (filter != NULL && strcmp(lua_tostring(event, 1), filter) != 0 && strcmp(lua_tostring(event, 1), "terminate") != 0);
    int count = lua_gettop(event);
    lua_xmove(event, L, count);
    lua_remove(paramQueue, 1);
    return count;
}

static bool forceLocalAlloc = false;

extern "C" void* lua_newuserdata_no_psram(lua_State *L, size_t size) {
    bool oldAlloc = forceLocalAlloc;
    forceLocalAlloc = true;
    void* ptr = lua_newuserdata(L, size);
    forceLocalAlloc = oldAlloc;
    return ptr;
}

static void *l_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud; (void)osize;  /* not used */
  if (nsize == 0) {
    if (((ptrdiff_t)ptr & 0xFF000000) == 0x11000000)
        sfe_mem_free(ptr);
    else
        free(ptr);
    return NULL;
  }
  else if ((ptr == NULL && forceLocalAlloc) || (ptr != NULL && ((ptrdiff_t)ptr & 0xFF000000) != 0x11000000))
    return realloc(ptr, nsize);
  else
    return sfe_mem_realloc(ptr, nsize);
}

void machine_main(void*) {
    int status;
    lua_State *L;
    lua_State *coro;
    /*
     * All Lua contexts are held in this structure. We work with it almost
     * all the time.
     */
    if (!sfe_pico_alloc_init()) forceLocalAlloc = true;
    L = lua_newstate(l_alloc, NULL);
    
    coro = lua_newthread(L);
    paramQueue = lua_newthread(L);
    
    luaL_openlibs(coro);
    lua_getglobal(coro, "os"); lua_getfield(coro, -1, "date");
    lua_newtable(coro); luaL_setfuncs(coro, fs_lib, 0); lua_setglobal(coro, "fs");
    lua_newtable(coro); luaL_setfuncs(coro, os_lib, 0); lua_pushvalue(coro, -2); lua_setfield(coro, -2, "date"); lua_setglobal(coro, "os");
    lua_newtable(coro); luaL_setfuncs(coro, peripheral_lib, 0); lua_setglobal(coro, "peripheral");
    lua_newtable(coro); luaL_setfuncs(coro, rs_lib, 0); lua_pushvalue(coro, -1); lua_setglobal(coro, "rs"); lua_setglobal(coro, "redstone");
    lua_newtable(coro); luaL_setfuncs(coro, term_lib, 0); lua_setglobal(coro, "term");
    lua_pop(coro, 2);
    
    lua_pushliteral(coro, "bios.use_multishell=false,shell.autocomplete=false");
    lua_setglobal(coro, "_CC_DEFAULT_SETTINGS");
    lua_pushliteral(coro, "ComputerCraft 1.109.2 (CraftOS-ESP 1.0)");
    lua_setglobal(coro, "_HOST");
    lua_pushnil(coro); lua_setglobal(coro, "package");
    lua_pushnil(coro); lua_setglobal(coro, "require");
    lua_pushnil(coro); lua_setglobal(coro, "io");
    
    /* Load the file containing the script we are going to run */
    printf("Loading BIOS...\n");
    int fd = mmfs_open(rom, "bios.lua", O_RDONLY, 0);
    const void* buf;
    size_t sz = mmfs_getbuf(rom, fd, &buf);
    mmfs_close(rom, fd);
    status = luaL_loadbuffer(coro, (const char*)buf, sz, "@bios.lua");
    if (status) {
        /* If something went wrong, error message is at the top of */
        /* the stack */
        const char * fullstr = lua_tostring(coro, -1);
        printf("Couldn't load BIOS: %s (%d)\n", fullstr, status);
        lua_close(L);
        terminal_clear(-1, 0xFE);
        terminal_write_literal(0, 0, "Error loading BIOS", 0xFE);
        terminal_write_string(0, 1, fullstr, 0xFE);
        terminal_write_literal(0, 2, "ComputerCraft may be installed incorrectly", 0xFE);
        while (true) vTaskDelay(pdMS_TO_TICKS(1000));
        return;
    }
    
    /* Ask Lua to run our little script */
    status = LUA_YIELD;
    int narg = 0;
    printf("Running main coroutine.\n");
    while (status == LUA_YIELD) {
        status = lua_resume(coro, NULL, narg);
        if (status == LUA_YIELD) {
            //printf("Yield\n");
            if (lua_isstring(coro, -1)) narg = getNextEvent(coro, lua_tostring(coro, -1));
            else narg = getNextEvent(coro, NULL);
        } else if (status != 0) {
            const char * fullstr = lua_tostring(coro, -1);
            printf("Errored: %s\n", fullstr);
            lua_close(L);
            terminal_clear(-1, 0xFE);
            terminal_write_literal(0, 0, "Error running computer", 0xFE);
            terminal_write_string(0, 1, fullstr, 0xFE);
            terminal_write_literal(0, 2, "ComputerCraft may be installed incorrectly", 0xFE);
            while (true) vTaskDelay(pdMS_TO_TICKS(1000));
            return;
        }
    }
    printf("Closing session.\n");
    lua_close(L);
    terminal_clear(-1, 0xFE);
    terminal_write_literal(0, 0, "Error running computer", 0xFE);
    terminal_write_literal(0, 1, "ComputerCraft may be installed incorrectly", 0xFE);
    while (true) vTaskDelay(pdMS_TO_TICKS(1000));
}
