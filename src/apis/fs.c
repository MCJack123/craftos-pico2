#include <ctype.h>
#include <errno.h>
#include <lua.h>
#include <lauxlib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sfe_pico_alloc.h>
#include <sfe_psram.h>
#include "fs_handle.h"
#include "../modules/mmfs.h"
#include <lfs.h>
#include <pico/flash.h>
#include <hardware/flash.h>
#include <sfe_pico_boards.h>

struct mmfs_mount* rom;
lfs_t root;
extern const unsigned char craftos2_rom[];

extern void* lua_newuserdata_no_psram(lua_State *L, size_t size);

typedef struct {
    int type; // 0 = closed, 1 = MMFS, 2 = LittleFS
    union {
        int mmfs_fd;
        lfs_file_t lfs_fp;
    };
} FILEHANDLE;

extern int sort(lua_State *L);

char* dirname(char* path) {
    char tch;
    if (strrchr(path, '/') != NULL) tch = '/';
    else if (strrchr(path, '\\') != NULL) tch = '\\';
    else {
        path[0] = '\0';
        return path;
    }
    path[strrchr(path, tch) - path] = '\0';
	return path;
}

const char* basename(const char* path) {
    const char* s = strrchr(path, '/');
    if (s) return s + 1;
    else return path;
}

char * unconst(const char * str) {
    char * retval = malloc(strlen(str) + 1);
    strcpy(retval, str);
    return retval;
}

static inline bool allchar(const char* str, char c) {
    while (*str) if (*str++ != c) return false;
    return true;
}

char* fixpath(const char* pat) {
    char* path = unconst(pat);
    char* retval = (char*)malloc(strlen(path) + 2);
    retval[0] = 0;
    for (char* tok = strtok(path, "/\\"); tok; tok = strtok(NULL, "/\\")) {
        if (strcmp(tok, "") == 0) continue;
        else if (strcmp(tok, "..") == 0) {
            char* p = strrchr(retval, '/');
            if (p) *p = '\0';
            else strcpy(retval, "..");
        } else if (allchar(tok, '.')) continue;
        else {
            strcat(retval, "/");
            strcat(retval, tok);
        }
    }
    free(path);
    if (strcmp(retval, "") == 0) strcpy(retval, "/");
    return retval;
}

void err(lua_State *L, char * path, const char * err) {
    char * msg = (char*)malloc(strlen(path) + strlen(err) + 3);
    sprintf(msg, "%s: %s", path, err);
    free(path);
    lua_pushstring(L, msg);
    lua_error(L);
}

int fs_list(lua_State *L) {
    int i;
    char * path = fixpath(lua_tostring(L, 1));
    if (strncmp(path, "/rom/", 5) == 0 || strcmp(path, "/rom") == 0) {
        struct dirent *dir;
        DIR * d = mmfs_opendir(rom, path + 4);
        if (d) {
            lua_newtable(L);
            for (i = 1; (dir = mmfs_readdir(rom, d)) != NULL; i++) {
                lua_pushinteger(L, i);
                lua_pushstring(L, dir->d_name);
                lua_settable(L, -3);
            }
            mmfs_closedir(rom, d);
        } else err(L, path, "Not a directory");
    } else {
        lfs_dir_t d;
        struct lfs_info dir;
        int erro = lfs_dir_open(&root, &d, path);
        if (erro == 0) {
            lua_newtable(L);
            for (i = 1; lfs_dir_read(&root, &d, &dir) > 0; i++) {
                lua_pushinteger(L, i);
                lua_pushstring(L, dir.name);
                lua_settable(L, -3);
            }
            lfs_dir_close(&root, &d);
            // apparently not needed anymore?
            if (strcmp(path, "/") == 0) {
                lua_pushinteger(L, i);
                lua_pushliteral(L, "rom");
                lua_settable(L, -3);
                // if (diskMounted) {
                //     lua_pushinteger(L, i);
                //     lua_pushliteral(L, "disk");
                //     lua_settable(L, -3);
                // }
            }
        } else err(L, path, "Not a directory");
    }
    free(path);
    lua_pushcfunction(L, sort);
    lua_pushvalue(L, -2);
    lua_call(L, 1, 0);
    return 1;
}

int fs_exists(lua_State *L) {
    struct stat st;
    struct lfs_info info;
    char * path = fixpath(lua_tostring(L, 1));
    lua_pushboolean(L, strcmp(path, "/rom") == 0 ? true : (strncmp(path, "/rom/", 5) == 0 ? mmfs_stat(rom, path + 4, &st) : lfs_stat(&root, path, &info)) == 0);
    free(path);
    return 1;
}

int fs_isDir(lua_State *L) {
    char * path = fixpath(lua_tostring(L, 1));
    if (strcmp(path, "/rom") == 0) lua_pushboolean(L, true);
    else if (strncmp(path, "/rom/", 5) == 0) {
        struct stat st;
        lua_pushboolean(L, mmfs_stat(rom, path + 4, &st) == 0 && S_ISDIR(st.st_mode));
    } else {
        struct lfs_info st;
        lua_pushboolean(L, lfs_stat(&root, path, &st) == 0 && st.type == LFS_TYPE_DIR);
    }
    free(path);
    return 1;
}

static bool isReadOnly(const char* path) {
    struct lfs_info info;
    if (strcmp(path, "/rom") == 0 || strcmp(path, "rom") == 0) return true;
    else if (strcmp(path, "/disk") == 0 || strcmp(path, "disk") == 0) return false;
    else if (strcmp(path, "/") == 0 || strcmp(path, "") == 0) return false;
    if (lfs_stat(&root, path, &info) != 0) {
        char* p = unconst(path);
        bool retval = isReadOnly(dirname(p));
        free(p);
        return retval;
    }
    return lfs_stat(&root, path, &info) != 0;
}

int fs_isReadOnly(lua_State *L) {
    char * path = fixpath(lua_tostring(L, 1));
    lua_pushboolean(L, isReadOnly(path));
    free(path);
    return 1;
}

int fs_getName(lua_State *L) {
    char * path = unconst(lua_tostring(L, 1));
    lua_pushstring(L, basename(path));
    free(path);
    return 1;
}

int fs_getDrive(lua_State *L) {
    char * path = fixpath(lua_tostring(L, 1));
    if (strncmp(path, "/rom/", 5) == 0) lua_pushliteral(L, "rom");
    else if (strncmp(path, "/disk/", 6) == 0) lua_pushliteral(L, "disk");
    else lua_pushliteral(L, "hdd");
    free(path);
    return 1;
}

int fs_getSize(lua_State *L) {
    char * path = fixpath(lua_tostring(L, 1));
    if (strncmp(path, "/rom/", 5) == 0) {
        struct stat st;
        if (mmfs_stat(rom, path + 4, &st) != 0) err(L, path, "No such file");
        lua_pushinteger(L, st.st_size);
    } else {
        struct lfs_info st;
        if (lfs_stat(&root, path, &st) != 0) err(L, path, "No such file");
        lua_pushinteger(L, st.size);
    }
    free(path);
    return 1;
}

size_t capacity_cache[2] = {0, 0}, free_space_cache[2] = {0, 0};

int fs_getFreeSpace(lua_State *L) {
    char * path = fixpath(lua_tostring(L, 1));
    size_t cap, used;
    if (strncmp(path, "/rom/", 5) == 0 || strcmp(path, "/rom") == 0) cap = 0, used = 0;
    else if (strncmp(path, "/disk", 5) == 0) {
        // if (free_space_cache[0]) {used = free_space_cache[0]; cap = capacity_cache[0];}
        // else {
        //     esp_vfs_fat_info("/disk", &cap, &used);
        //     capacity_cache[0] = cap;
        //     free_space_cache[0] = used;
        // }
    } else {
        if (free_space_cache[1]) {used = free_space_cache[1]; cap = capacity_cache[1];}
        else {
            struct lfs_fsinfo info;
            lfs_fs_stat(&root, &info);
            used = lfs_fs_size(&root);
            cap = info.block_count * info.block_size;
            capacity_cache[1] = cap;
            free_space_cache[1] = used;
        }
    }
    lua_pushinteger(L, cap - used);
    return 1;
}

int fs_getCapacity(lua_State *L) {
    char * path = fixpath(lua_tostring(L, 1));
    size_t cap, used;
    if (strncmp(path, "/rom/", 5) == 0 || strcmp(path, "/rom") == 0) cap = 0, used = 0;
    else if (strncmp(path, "/disk", 5) == 0) {
        // if (capacity_cache[0]) cap = capacity_cache[0];
        // else {
        //     esp_vfs_fat_info("/disk", &cap, &used);
        //     capacity_cache[0] = cap;
        //     free_space_cache[0] = used;
        // }
    } else {
        if (capacity_cache[1]) cap = capacity_cache[1];
        else {
            struct lfs_fsinfo info;
            lfs_fs_stat(&root, &info);
            used = lfs_fs_size(&root);
            cap = info.block_count * info.block_size;
            capacity_cache[1] = cap;
            free_space_cache[1] = used;
        }
    }
    lua_pushinteger(L, cap);
    return 1;
}

int recurse_mkdir(const char * path) {
    char* d = unconst(path);
    struct lfs_info st;
    dirname(d);
    if (strcmp(path, "") != 0 && strcmp(d, "") != 0 && lfs_stat(&root, d, &st) != 0) {
        int err = recurse_mkdir(d);
        if (err != 0) {
            free(d);
            return err;
        }
    }
    free(d);
    return lfs_mkdir(&root, path);
}

int fs_makeDir(lua_State *L) {
    char * path = fixpath(lua_tostring(L, 1));
    if (strcmp(path, "/rom") == 0 || strncmp(path, "/rom/", 5) == 0) err(L, path, "Permission denied");
    if (recurse_mkdir(path) != 0) err(L, path, strerror(errno));
    free(path);
    free_space_cache[0] = free_space_cache[1] = 0;
    return 0;
}

int fs_move(lua_State *L) {
    char * fromPath, *toPath;
    fromPath = fixpath(lua_tostring(L, 1));
    toPath = fixpath(lua_tostring(L, 2));
    if (strcmp(fromPath, "/rom") == 0 || strncmp(fromPath, "/rom/", 5) == 0 || strcmp(toPath, "/rom") == 0 || strncmp(toPath, "/rom/", 5) == 0) {
        free(toPath);
        err(L, fromPath, "Permission denied");
    }
    char* dir = unconst(toPath);
    if (strcmp(dirname(dir), "") != 0) recurse_mkdir(dir);
    free(dir);
    if (rename(fromPath, toPath) != 0) {
        free(toPath);
        err(L, fromPath, strerror(errno));
    }
    free(fromPath);
    free(toPath);
    return 0;
}

static int aux_copy(const char* fromPath, const char* toPath) {
    static char tmp[1024];
    int read;
    if (strcmp(fromPath, "/rom") == 0 || strncmp(fromPath, "/rom/", 5) == 0) {
        struct stat st;
        if (mmfs_stat(rom, fromPath + 4, &st) != 0) return -1;
        if (S_ISDIR(st.st_mode)) {
            struct dirent *dir;
            int i;
            DIR * d;
            d = mmfs_opendir(rom, fromPath + 4);
            if (d) {
                for (i = 1; (dir = mmfs_readdir(rom, d)) != NULL; i++) {
                    char* fp = malloc(strlen(fromPath) + strlen(dir->d_name) + 2);
                    char* tp = malloc(strlen(toPath) + strlen(dir->d_name) + 2);
                    strcpy(fp, fromPath);
                    strcat(fp, "/");
                    strcat(fp, dir->d_name);
                    strcpy(tp, toPath);
                    strcat(tp, "/");
                    strcat(tp, dir->d_name);
                    int err = aux_copy(fp, tp);
                    free(fp);
                    free(tp);
                    if (err) return err;
                }
                mmfs_closedir(rom, d);
            } else return -1;
        } else {
            int fromfd, erro;
            lfs_file_t tofp;
            char* dir = unconst(toPath);
            if (strcmp(dirname(dir), "") != 0) recurse_mkdir(dir);
            free(dir);
            fromfd = mmfs_open(rom, fromPath + 4, O_RDONLY, 0);
            if (fromfd < 0) {
                return -1;
            }
            erro = lfs_file_open(&root, &tofp, toPath, LFS_O_WRONLY);
            if (erro < 0) {
                mmfs_close(rom, fromfd);
                return -1;
            }

            do {
                read = mmfs_read(rom, fromfd, tmp, 1024);
                if (read > 0) lfs_file_write(&root, &tofp, tmp, read);
            } while (read == 1024);

            mmfs_close(rom, fromfd);
            lfs_file_close(&root, &tofp);
        }
    } else {
        struct lfs_info st;
        if (lfs_stat(&root, fromPath, &st) != 0) return -1;
        if (st.type == LFS_TYPE_DIR) {
            struct lfs_info dir;
            int i;
            lfs_dir_t d;
            int erro = lfs_dir_open(&root, &d, fromPath);
            if (erro == 0) {
                for (i = 1; lfs_dir_read(&root, &d, &dir) == 0; i++) {
                    char* fp = malloc(strlen(fromPath) + strlen(dir.name) + 2);
                    char* tp = malloc(strlen(toPath) + strlen(dir.name) + 2);
                    strcpy(fp, fromPath);
                    strcat(fp, "/");
                    strcat(fp, dir.name);
                    strcpy(tp, toPath);
                    strcat(tp, "/");
                    strcat(tp, dir.name);
                    int err = aux_copy(fp, tp);
                    free(fp);
                    free(tp);
                    if (err) return err;
                }
                lfs_dir_close(&root, &d);
            } else return -1;
        } else {
            int erro;
            lfs_file_t fromfp, tofp;
            char* dir = unconst(toPath);
            if (strcmp(dirname(dir), "") != 0) recurse_mkdir(dir);
            free(dir);
            erro = lfs_file_open(&root, &fromfp, fromPath, LFS_O_RDONLY);
            if (erro < 0) {
                return -1;
            }
            erro = lfs_file_open(&root, &tofp, toPath, LFS_O_WRONLY);
            if (erro < 0) {
                lfs_file_close(&root, &fromfp);
                return -1;
            }

            do {
                read = lfs_file_read(&root, &fromfp, tmp, 1024);
                if (read > 0) lfs_file_write(&root, &tofp, tmp, read);
            } while (read == 1024);

            lfs_file_close(&root, &fromfp);
            lfs_file_close(&root, &tofp);
        }
    }
    return 0;
}

int fs_copy(lua_State *L) {
    char* fromPath = fixpath(lua_tostring(L, 1));
    char* toPath = fixpath(lua_tostring(L, 2));
    if (strcmp(toPath, "/rom") == 0 || strncmp(toPath, "/rom/", 5) == 0) {
        free(fromPath);
        err(L, toPath, "Permission denied");
    }
    if (aux_copy(fromPath, toPath)) {
        free(toPath);
        err(L, fromPath, "Failed to copy");
    }
    free(fromPath);
    free(toPath);
    free_space_cache[0] = free_space_cache[1] = 0;
    return 0;
}

int aux_delete(const char* path) {
    struct lfs_info st;
    if (lfs_stat(&root, path, &st) != 0) return -1;
    if (st.type == LFS_TYPE_DIR) {
        struct lfs_info dir;
        int i;
        lfs_dir_t d;
        int ok = 0;
        int erro = lfs_dir_open(&root, &d, path);
        if (erro == 0) {
            for (i = 1; lfs_dir_read(&root, &d, &dir) > 0; i++) {
                char* p = malloc(strlen(path) + strlen(dir.name) + 2);
                strcpy(p, path);
                strcat(p, "/");
                strcat(p, dir.name);
                ok = aux_delete(p) || ok;
                free(p);
            }
            lfs_dir_close(&root, &d);
            lfs_remove(&root, path);
            return ok;
        } else return -1;
    } else return lfs_remove(&root, path);
}

int fs_delete(lua_State *L) {
    char * path = fixpath(lua_tostring(L, 1));
    if (strcmp(path, "/rom") == 0 || strncmp(path, "/rom/", 5) == 0) {
        err(L, path, "Permission denied");
    }
    if (aux_delete(path) != 0) err(L, path, strerror(errno));
    free(path);
    free_space_cache[0] = free_space_cache[1] = 0;
    return 0;
}

int fs_combine(lua_State *L) {
    char * basePath = unconst(luaL_checkstring(L, 1));
    for (int i = 2; i <= lua_gettop(L); i++) {
        if (!lua_isstring(L, i)) {
            free(basePath);
            luaL_checkstring(L, i);
            return 0;
        }
        const char * str = lua_tostring(L, i);
        char* ns = malloc(strlen(basePath)+strlen(str)+3);
        strcpy(ns, basePath);
        if (strlen(basePath) > 0 && basePath[strlen(basePath)-1] == '/' && str[0] == '/') strcat(ns, str + 1);
        else {
            if (strlen(basePath) && basePath[strlen(basePath)-1] != '/' && str[0] != '/') strcat(ns, "/");
            strcat(ns, str);
        }
        free(basePath);
        basePath = ns;
    }
    char* newPath = fixpath(basePath);
    lua_pushstring(L, newPath + 1);
    free(newPath);
    free(basePath);
    return 1;
}

static int fs_open(lua_State *L) {
    const char * mode = luaL_checkstring(L, 2);
    if ((mode[0] != 'r' && mode[0] != 'w' && mode[0] != 'a') || (mode[1] != 'b' && mode[1] != '\0')) luaL_error(L, "%s: Unsupported mode", mode);
    char * path = fixpath(luaL_checkstring(L, 1));
    FILEHANDLE * fp = (FILEHANDLE*)lua_newuserdata_no_psram(L, sizeof(FILEHANDLE));
    int fpid = lua_gettop(L);
    if (strcmp(path, "/rom") == 0 || strncmp(path, "/rom/", 5) == 0) {
        if (mode[0] != 'r') {
            lua_pushnil(L);
            lua_pushfstring(L, "%s: Permission denied", path);
            free(path);
            return 2;
        }
        fp->type = 1;
        if ((fp->mmfs_fd = mmfs_open(rom, path + 4, O_RDONLY, 0)) < 0) {
            lua_remove(L, fpid);
            lua_pushnil(L);
            lua_pushfstring(L, "%s: No such file", path);
            free(path);
            return 2; 
        }
    } else {
        struct lfs_info st;
        int err = lfs_stat(&root, path, &st);
        if (err != 0 && mode[0] == 'r') {
            lua_pushnil(L);
            lua_pushfstring(L, "%s: No such file", path);
            free(path);
            return 2;
        } else if (err == 0 && st.type == LFS_TYPE_DIR) { 
            lua_pushnil(L);
            if (strcmp(mode, "r") == 0 || strcmp(mode, "rb") == 0) lua_pushfstring(L, "%s: No such file", path);
            else lua_pushfstring(L, "%s: Cannot write to directory", path);
            free(path);
            return 2;
        }
        if (mode[0] == 'w' || mode[0] == 'a') {
            char* dir = unconst(path);
            if (strcmp(dirname(dir), "") != 0) recurse_mkdir(dir);
            free(dir);
        }
        fp->type = 2;
        if ((err = lfs_file_open(&root, &fp->lfs_fp, path, (mode[0] == 'r' ? LFS_O_RDONLY : LFS_O_WRONLY | LFS_O_CREAT) | (mode[0] == 'a' ? LFS_O_APPEND : LFS_O_TRUNC))) < 0) {
            lua_remove(L, fpid);
            lua_pushnil(L);
            lua_pushfstring(L, "%s: No such file", path);
            free(path);
            return 2;
        }
    }
    lua_createtable(L, 0, 4);
    lua_pushstring(L, "close");
    lua_pushvalue(L, fpid);
    lua_pushcclosure(L, fs_handle_close, 1);
    lua_settable(L, -3);
    if (strcmp(mode, "r") == 0 || strcmp(mode, "rb") == 0) {
        lua_pushstring(L, "readAll");
        lua_pushvalue(L, fpid);
        lua_pushcclosure(L, fs_handle_readAllByte, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "readLine");
        lua_pushvalue(L, fpid);
        lua_pushboolean(L, false);
        lua_pushcclosure(L, fs_handle_readLine, 2);
        lua_settable(L, -3);

        lua_pushstring(L, "read");
        lua_pushvalue(L, fpid);
        lua_pushcclosure(L, strcmp(mode, "rb") == 0 ? fs_handle_readByte : fs_handle_readChar, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "seek");
        lua_pushvalue(L, fpid);
        lua_pushcclosure(L, fs_handle_seek, 1);
        lua_settable(L, -3);
    } else if (strcmp(mode, "w") == 0 || strcmp(mode, "a") == 0 || strcmp(mode, "wb") == 0 || strcmp(mode, "ab") == 0) {
        lua_pushstring(L, "write");
        lua_pushvalue(L, fpid);
        lua_pushcclosure(L, fs_handle_writeByte, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "writeLine");
        lua_pushvalue(L, fpid);
        lua_pushcclosure(L, fs_handle_writeLine, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "flush");
        lua_pushvalue(L, fpid);
        lua_pushcclosure(L, fs_handle_flush, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "seek");
        lua_pushvalue(L, fpid);
        lua_pushcclosure(L, fs_handle_seek, 1);
        lua_settable(L, -3);
    }
    free(path);
    return 1;
}

int fs_getDir(lua_State *L) {
    char * path = unconst(lua_tostring(L, 1));
    dirname(path);
    lua_pushstring(L, path[0] == '/' ? path + 1 : path);
    free(path);
    return 1;
}

#define st_time_ms(st) ((st##time_nsec / 1000000) + (st##time * 1000.0))

int fs_attributes(lua_State *L) {
    char* path = fixpath(lua_tostring(L, 1));
    struct stat st;
    struct lfs_info info;
    if (strcmp(path, "/rom") == 0) {
        lua_createtable(L, 0, 6);
        lua_pushinteger(L, 0);
        lua_setfield(L, -2, "modification");
        lua_pushinteger(L, 0);
        lua_setfield(L, -2, "modified");
        lua_pushinteger(L, 0);
        lua_setfield(L, -2, "created");
        lua_pushinteger(L, 0);
        lua_setfield(L, -2, "size");
        lua_pushboolean(L, true);
        lua_setfield(L, -2, "isDir");
        lua_pushboolean(L, true);
        lua_setfield(L, -2, "isReadOnly");
        return 1;
    } else if (strncmp(path, "/rom/", 5) == 0) {
        if (mmfs_stat(rom, path + 4, &st) != 0) {
            lua_pushnil(L);
            return 1;
        }
    } else {
        if (lfs_stat(&root, path, &info) != 0) {
            lua_pushnil(L);
            return 1;
        }
    }
    lua_createtable(L, 0, 6);
    lua_pushinteger(L, 0);
    lua_setfield(L, -2, "modification");
    lua_pushinteger(L, 0);
    lua_setfield(L, -2, "modified");
    lua_pushinteger(L, 0);
    lua_setfield(L, -2, "created");
    if (strncmp(path, "/rom/", 5) == 0) {
        lua_pushinteger(L, S_ISDIR(st.st_mode) ? 0 : st.st_size);
        lua_setfield(L, -2, "size");
        lua_pushboolean(L, S_ISDIR(st.st_mode));
        lua_setfield(L, -2, "isDir");
        lua_pushboolean(L, true);
        lua_setfield(L, -2, "isReadOnly");
    } else {
        lua_pushinteger(L, info.type == LFS_TYPE_DIR ? 0 : info.size);
        lua_setfield(L, -2, "size");
        lua_pushboolean(L, info.type == LFS_TYPE_DIR);
        lua_setfield(L, -2, "isDir");
        lua_pushboolean(L, false);
        lua_setfield(L, -2, "isReadOnly");
    }
    free(path);
    return 1;
}

const void* FLASH_PARTITION = (const void*)0x10200000;
static bool psramActive = true;

static int flash_read(const struct lfs_config *c, lfs_block_t block,
            lfs_off_t off, void *buffer, lfs_size_t size) {
    memcpy(buffer, FLASH_PARTITION + block * c->block_size + off, size);
    return 0;
}

struct flash_prog {
    lfs_block_t block;
    lfs_off_t off;
    const void *buffer;
    lfs_size_t size;
};

static void _flash_prog(void* info) {
    struct flash_prog* prog = info;
    if (psramActive) {
        // flush XIP cache to PSRAM
        for (volatile uint8_t* cache = (volatile uint8_t*)0x18000001; cache < (volatile uint8_t*)(0x18000001 + 2048 * 8); cache += 8)
            *cache = 0;
        psramActive = false;
    }
    flash_range_program((prog->block + 512) * 4096 + prog->off, prog->buffer, prog->size);
}

static int flash_prog(const struct lfs_config *c, lfs_block_t block,
            lfs_off_t off, const void *buffer, lfs_size_t size) {
    struct flash_prog prog = {block, off, buffer, size};
    return flash_safe_execute(_flash_prog, &prog, 100);
}

static void _flash_erase(void* addr) {
    if (psramActive) {
        // flush XIP cache to PSRAM
        for (volatile uint8_t* cache = (volatile uint8_t*)0x18000001; cache < (volatile uint8_t*)(0x18000001 + 2048 * 8); cache += 8)
            *cache = 0;
        psramActive = false;
    }
    flash_range_erase((uint32_t)(addr + 512) * 4096, 1);
}

static int flash_erase(const struct lfs_config *c, lfs_block_t block) {
    return flash_safe_execute(_flash_erase, (void*)block, 100);
}

static int flash_sync(const struct lfs_config *c) {
    sfe_setup_psram(SFE_RP2350_XIP_CSI_PIN);
    psramActive = true;
    return 0;
}

static struct lfs_config config = {
    // block device operations
    .read  = flash_read,
    .prog  = flash_prog,
    .erase = flash_erase,
    .sync  = flash_sync,

    // block device configuration
    .read_size = 1,
    .prog_size = 256,
    .block_size = 4096,
    .block_count = 256,
    .cache_size = 256,
    .lookahead_size = 16,
    .block_cycles = 500,
};

void fs_init(void) {
    rom = mmfs_mount(craftos2_rom);
    if (lfs_mount(&root, &config) < 0) {
        lfs_format(&root, &config);
        lfs_mount(&root, &config);
    }
}

const luaL_Reg fs_lib[] = {
    {"list", fs_list},
    {"exists", fs_exists},
    {"isDir", fs_isDir},
    {"isReadOnly", fs_isReadOnly},
    {"getName", fs_getName},
    {"getDrive", fs_getDrive},
    {"getSize", fs_getSize},
    {"getFreeSpace", fs_getFreeSpace},
    {"getCapacity", fs_getCapacity},
    {"makeDir", fs_makeDir},
    {"move", fs_move},
    {"copy", fs_copy},
    {"delete", fs_delete},
    {"combine", fs_combine},
    {"open", fs_open},
    {"getDir", fs_getDir},
    {"attributes", fs_attributes},
    {NULL, NULL}
};
