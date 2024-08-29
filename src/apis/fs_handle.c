#include <errno.h>
#include <lauxlib.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sfe_pico_alloc.h>
#include "fs_handle.h"
#include "../modules/mmfs.h"
#include <lfs.h>

extern size_t free_space_cache[2];
extern struct mmfs_mount* rom;
extern lfs_t root;

typedef struct {
    int type;
    union {
        int mmfs_fd;
        lfs_file_t lfs_fp;
    };
} FILEHANDLE;

int fs_handle_close(lua_State *L) {
    FILEHANDLE* handle = (FILEHANDLE*)lua_touserdata(L, lua_upvalueindex(1));
    if (!handle->type)
        return luaL_error(L, "attempt to use a closed file");
    switch (handle->type) {
        case 1: mmfs_close(rom, handle->mmfs_fd); break;
        case 2: lfs_file_close(&root, &handle->lfs_fp); break;
    }
    handle->type = 0;
    free_space_cache[0] = free_space_cache[1] = 0;
    return 0;
}

int fs_handle_readAllByte(lua_State *L) {
    FILEHANDLE* handle = (FILEHANDLE*)lua_touserdata(L, lua_upvalueindex(1));
    if (!handle->type) luaL_error(L, "attempt to use a closed file");
    switch (handle->type) {
        case 1: {
            const long pos = mmfs_lseek(rom, handle->mmfs_fd, 0, SEEK_CUR);
            const long end = mmfs_lseek(rom, handle->mmfs_fd, 0, SEEK_END);
            mmfs_lseek(rom, handle->mmfs_fd, pos, SEEK_SET);
            const long size = end - pos;
            if (size == 0) return 0;
            char * retval = malloc(size);
            mmfs_read(rom, handle->mmfs_fd, retval, size);
            lua_pushlstring(L, retval, size);
            free(retval);
            return 1;
        } case 2: {
            const long size = lfs_file_size(&root, &handle->lfs_fp) - lfs_file_tell(&root, &handle->lfs_fp);
            if (size == 0) return 0;
            char * retval = malloc(size);
            lfs_file_read(&root, &handle->lfs_fp, retval, size);
            lua_pushlstring(L, retval, size);
            free(retval);
            return 1;
        }
    }
}

int fs_handle_readLine(lua_State *L) {
    FILEHANDLE* handle = (FILEHANDLE*)lua_touserdata(L, lua_upvalueindex(1));
    if (!handle->type) luaL_error(L, "attempt to use a closed file");
    char* retval = (char*)malloc(256);
    retval[0] = 0;
    size_t len;
    for (unsigned i = 0; 1; i += 256) {
        ssize_t actual;
        switch (handle->type) {
            case 1: actual = mmfs_read(rom, handle->mmfs_fd, &retval[i], 256); break;
            case 2: actual = lfs_file_read(&root, &handle->lfs_fp, &retval[i], 256); break;
        }
        long found = 0;
        for (long j = 0; j < actual; j++) if (retval[i+j] == '\n') {found = j; break;}
        if (found) {
            switch (handle->type) {
                case 1: mmfs_lseek(rom, handle->mmfs_fd, found - (long)actual + 1, SEEK_CUR); break;
                case 2: lfs_file_seek(&root, &handle->lfs_fp, found - (long)actual + 1, LFS_SEEK_CUR); break;
            }
            len = i + found + (lua_toboolean(L, 1) ? 1 : 0);
            break;
        }
        if (actual < 256) {
            len = i + actual;
            break;
        }
        char * retvaln = (char*)realloc(retval, i + 512);
        if (retvaln == NULL) {
            free(retval);
            return luaL_error(L, "failed to allocate memory");
        }
        retval = retvaln;
    }
    if (len == 0) {free(retval); return 0;}
    lua_pushlstring(L, retval, len);
    free(retval);
    return 1;
}

int fs_handle_readChar(lua_State *L) {
    FILEHANDLE* handle = (FILEHANDLE*)lua_touserdata(L, lua_upvalueindex(1));
    if (!handle->type) luaL_error(L, "attempt to use a closed file");
    char c;
    switch (handle->type) {
        case 1: if (mmfs_read(rom, handle->mmfs_fd, &c, 1) < 1) return 0; break;
        case 2: if (lfs_file_read(&root, &handle->lfs_fp, &c, 1) < 1) return 0; break;
    }
    lua_pushlstring(L, &c, 1);
    return 1;
}

int fs_handle_readByte(lua_State *L) {
    FILEHANDLE* handle = (FILEHANDLE*)lua_touserdata(L, lua_upvalueindex(1));
    if (!handle->type) luaL_error(L, "attempt to use a closed file");
    if (lua_isnumber(L, 1)) {
        const size_t s = lua_tointeger(L, 1);
        if (s == 0) return 0;
        char* retval = malloc(s);
        size_t actual;
        switch (handle->type) {
            case 1: actual = mmfs_read(rom, handle->mmfs_fd, retval, s); break;
            case 2: actual = lfs_file_read(&root, &handle->lfs_fp, retval, s); break;
        }
        if (actual == 0) {free(retval); return 0;}
        lua_pushlstring(L, retval, actual);
        free(retval);
    } else {
        char c;
        switch (handle->type) {
            case 1: if (mmfs_read(rom, handle->mmfs_fd, &c, 1) < 1) return 0; break;
            case 2: if (lfs_file_read(&root, &handle->lfs_fp, &c, 1) < 1) return 0; break;
        }
        lua_pushinteger(L, c);
    }
    return 1;
}

int fs_handle_writeString(lua_State *L) {
    FILEHANDLE* handle = (FILEHANDLE*)lua_touserdata(L, lua_upvalueindex(1));
    if (!handle->type) luaL_error(L, "attempt to use a closed file");
    if (lua_isnoneornil(L, 1)) return 0;
    else if (!lua_isstring(L, 1) && !lua_isnumber(L, 1)) luaL_argerror(L, 1, "string expected");
    size_t sz = 0;
    const char * str = lua_tolstring(L, 1, &sz);
    switch (handle->type) {
        case 2: lfs_file_write(&root, &handle->lfs_fp, str, sz); break;
    }
    return 0;
}

int fs_handle_writeLine(lua_State *L) {
    FILEHANDLE* handle = (FILEHANDLE*)lua_touserdata(L, lua_upvalueindex(1));
    if (!handle->type) luaL_error(L, "attempt to use a closed file");
    if (lua_isnoneornil(L, 1)) return 0;
    else if (!lua_isstring(L, 1) && !lua_isnumber(L, 1)) luaL_argerror(L, 1, "string expected");
    size_t sz = 0;
    const char * str = lua_tolstring(L, 1, &sz);
    char c = '\n';
    switch (handle->type) {
        case 2: lfs_file_write(&root, &handle->lfs_fp, str, sz); lfs_file_write(&root, &handle->lfs_fp, &c, 1); break;
    }
    return 0;
}

int fs_handle_writeByte(lua_State *L) {
    FILEHANDLE* handle = (FILEHANDLE*)lua_touserdata(L, lua_upvalueindex(1));
    if (!handle->type) luaL_error(L, "attempt to use a closed file");
    if (lua_type(L, 1) == LUA_TNUMBER) {
        const char b = (unsigned char)(lua_tointeger(L, 1) & 0xFF);
        switch (handle->type) {
            case 2: lfs_file_write(&root, &handle->lfs_fp, &b, 1); break;
        }
    } else if (lua_isstring(L, 1)) {
        size_t sz;
        const char* str = lua_tolstring(L, 1, &sz);
        if (sz == 0) return 0;
        switch (handle->type) {
            case 2: lfs_file_write(&root, &handle->lfs_fp, str, sz); break;
        }
    } else luaL_argerror(L, 1, "number or string expected");
    return 0;
}

int fs_handle_flush(lua_State *L) {
    FILEHANDLE* handle = (FILEHANDLE*)lua_touserdata(L, lua_upvalueindex(1));
    if (!handle->type) luaL_error(L, "attempt to use a closed file");
    switch (handle->type) {
        case 2: lfs_file_sync(&root, &handle->lfs_fp); break;
    }
    free_space_cache[0] = free_space_cache[1] = 0;
    return 0;
}

int fs_handle_seek(lua_State *L) {
    FILEHANDLE* handle = (FILEHANDLE*)lua_touserdata(L, lua_upvalueindex(1));
    if (!handle->type) luaL_error(L, "attempt to use a closed file");
    const char * whence = luaL_optstring(L, 1, "cur");
    const long offset = (long)luaL_optinteger(L, 2, 0);
    int origin = 0;
    if (strcmp(whence, "set") == 0) origin = SEEK_SET;
    else if (strcmp(whence, "cur") == 0) origin = SEEK_CUR;
    else if (strcmp(whence, "end") == 0) origin = SEEK_END;
    else luaL_error(L, "bad argument #1 to 'seek' (invalid option '%s')", whence);
    switch (handle->type) {
        case 1: lua_pushinteger(L, mmfs_lseek(rom, handle->mmfs_fd, offset, origin)); break;
        case 2: lua_pushinteger(L, lfs_file_seek(&root, &handle->lfs_fp, offset, origin)); break;
    }
    return 1;
}
