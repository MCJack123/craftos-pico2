#ifndef EVENT_H
#define EVENT_H
#include <FreeRTOS.h>

typedef enum {
    EVENT_TYPE_NONE,
    EVENT_TYPE_KEY,
    EVENT_TYPE_KEY_UP,
    EVENT_TYPE_CHAR,
    EVENT_TYPE_TIMER,
    EVENT_TYPE_ALARM,
    EVENT_TYPE_DISK,
    EVENT_TYPE_DISK_EJECT,
    EVENT_TYPE_PASTE,
    EVENT_TYPE_REDSTONE,
    EVENT_TYPE_SPEAKER_AUDIO_EMPTY,
    EVENT_TYPE_TERMINATE,
    EVENT_TYPE_PERIPHERAL,
    EVENT_TYPE_PERIPHERAL_DETACH,
    EVENT_TYPE_WIFI_SCAN,
    EVENT_TYPE_WIFI_CONNECT,
    EVENT_TYPE_WIFI_DISCONNECT,
} event_type_t;

typedef struct {
    char ssid[33];
    uint8_t bars;
    uint8_t security;
} wifi_network_t;

typedef union {
    uint32_t type;
    struct {
        uint32_t type;
        uint8_t keycode;
        bool repeat;
    } key;
    struct {
        uint32_t type;
        char c;
    } character;
    struct {
        uint32_t type;
        uint32_t timerID;
    } timer;
    struct {
        uint32_t type;
        const char * side;
    } peripheral;
    struct {
        uint32_t type;
        uint16_t networkCount;
        wifi_network_t* networks;
    } wifi;
} event_t;

extern void event_push(const event_t* event);
extern BaseType_t event_push_isr(const event_t* event);
extern void event_wait(event_t* event);
extern void event_flush(void);

#endif