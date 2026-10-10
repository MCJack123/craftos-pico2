#include "peripheral.hpp"
#include "../drivers/audio.h"
#include "../event.hpp"
#include <FreeRTOS.h>
#include <task.h>
#include <stdlib.h>

struct speaker_state {
    audio_output_t output;
    bool isRight;
    TaskHandle_t task;
};

static void mixer_task(void* output) {
    while (true) {
        audio_mixer_step((audio_output_t)output);
        vTaskDelay(1);
    }
}

static void speaker_done(audio_output_t output, const uint8_t* samples, void* userp) {
    event_t event;
    event.type = EVENT_TYPE_SPEAKER_AUDIO_EMPTY;
    event.peripheral.side = userp ? "right" : "left";
    event_push(&event);
    free((void*)samples);
}

static int speaker_playAudio(lua_State *L) {
    speaker_state* speaker = (speaker_state*)lua_touserdata(L, 1);
    lua_remove(L, 1);
    luaL_checktype(L, 1, LUA_TTABLE);
    double volume = luaL_optnumber(L, 2, 1);
    size_t len = lua_rawlen(L, 1);
    if (len == 0 || len > 131072) luaL_error(L, "bad argument #1 (table too big)");
    if (!audio_has_unused_source(speaker->output)) {
        lua_pushboolean(L, false);
        return 1;
    }
    uint8_t *buf = (uint8_t*)malloc(len); /* needs to be DMA-able */
    if (buf == NULL) luaL_error(L, "out of memory");
    for (size_t i = 0; i < len; i++) {
        lua_rawgeti(L, 1, i + 1);
        if (!lua_isnumber(L, -1)) {
            free(buf);
            luaL_error(L, "bad argument #1 (index %d is not a number)", i + 1);
        }
        lua_Integer n = lua_tointeger(L, -1);
        if (n < -128 || n > 127) {
            free(buf);
            luaL_error(L, "bad argument #1 (index %d out of range)", i + 1);
        }
        buf[i] = n + 128;
        lua_pop(L, 1);
    }
    int id = audio_play_once(speaker->output, buf, len, speaker_done, NULL); /* TODO: get side */
    if (id == -1) {
        free(buf);
        lua_pushboolean(L, false);
        return 1;
    }
    audio_source_set_volume(speaker->output, id, volume * 256);
    lua_pushboolean(L, true);
    return 1;
}

static int speaker_stop(lua_State *L) {
    audio_source_stop(((speaker_state*)lua_touserdata(L, 1))->output, -1);
    return 0;
}

static speaker_state states[2];

static int speaker_open(bool right, void** userp_out) {
    speaker_state * speaker = &states[right ? 1 : 0];
    *userp_out = speaker;
    speaker->output = audio_init(right ? 33 : 9, 48000);
    if (speaker->output == NULL) return -1;
    speaker->isRight = right;
    xTaskCreate(mixer_task, "speaker", 512, speaker->output, 4, &speaker->task);
    return 0;
}

static void speaker_close(void* userp) {
    speaker_state * speaker = (speaker_state*)userp;
    vTaskDelete(speaker->task);
    audio_close(speaker->output);
}

static const luaL_Reg speaker_lib[] = {
    {"playAudio", speaker_playAudio},
    {"stop", speaker_stop},
    {NULL, NULL}
};

struct peripheral peripheral_speaker = {
    {"speaker", NULL},
    speaker_lib,
    speaker_open,
    speaker_close
};
