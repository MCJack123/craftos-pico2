#include <FreeRTOS.h>
#include <task.h>
#include <hardware/irq.h>
#include <hardware/pio.h>
#include <hardware/clocks.h>
#include <pico/time.h>
#include <craftos.h>
#include "../event.hpp"
#include "peripheral_id.pio.h"

struct peripheral {
    const char * types[2];
    const luaL_Reg * funcs;
    void* userp;
};

extern "C" {
    craftos_machine_t machine = NULL;
}

static TaskHandle_t detect_task_left, detect_task_right;
static struct peripheral * peripherals[8] = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

// static void unplug(void) {
//     // do nothing
//     event_t event;
//     event.type = EVENT_TYPE_CHAR;
//     event.character.c = 'X';
//     event_push_isr(&event);
// }

static void peripheral_detect_task(void* isRight) {
    static const uint led_pin = 4; //isRight ? 36 : 7;
    static const float pio_freq = 488.28125 * 8;

    // Choose PIO instance (0 or 1)
    PIO pio = pio0;

    // Get first free state machine in PIO 0
    uint sm = pio_claim_unused_sm(pio, true);

    // Add PIO program to PIO instruction memory. SDK will find location and
    // return with the memory offset of the program.
    uint offset = pio_add_program(pio, &peripheral_id_decoder_program);

    // Calculate the PIO clock divider
    float div = (float)clock_get_hz(clk_sys) / pio_freq;

    // Initialize the program using the helper function in our .pio file
    peripheral_id_decoder_program_init(pio, sm, offset, led_pin, div);

    //irq_set_exclusive_handler(pio_get_irq_num(pio, 0), unplug);
    //irq_set_enabled(pio_get_irq_num(pio, 0), true);

    // Start running our PIO program in the state machine
    pio_sm_set_enabled(pio, sm, true);

    absolute_time_t last_check = 0;
    int last_id = -1;
    while (true) {
        while (pio_sm_is_rx_fifo_empty(pio, sm)) {
            if (last_id != -1 && absolute_time_diff_us(get_absolute_time(), last_check) > 500000) {
                event_t event;
                event.type = EVENT_TYPE_PERIPHERAL_DETACH;
                event.peripheral.side = isRight ? "right" : "left";
                event_push(&event);
                craftos_machine_peripheral_detach(machine, isRight ? "right" : "left");
                last_id = -1;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        uint32_t id = pio_sm_get_blocking(pio, sm) & 7;
        last_check = get_absolute_time();
        if (last_id != -1 && last_id != id) {
            event_t event;
            event.type = EVENT_TYPE_PERIPHERAL_DETACH;
            event.peripheral.side = isRight ? "right" : "left";
            event_push(&event);
            craftos_machine_peripheral_detach(machine, isRight ? "right" : "left");
            last_id = -1;
        }
        if (last_id == -1) {
            if (peripherals[id] != NULL) {
                event_t event;
                event.type = EVENT_TYPE_PERIPHERAL;
                event.peripheral.side = isRight ? "right" : "left";
                event_push(&event);
                craftos_machine_peripheral_attach(machine, isRight ? "right" : "left", peripherals[id]->types, peripherals[id]->funcs, peripherals[id]->userp);
            }
            last_id = id;
        }
    }
}

void peripheral_detect_init() {
    xTaskCreate(peripheral_detect_task, "peripheral_detect_left", 1536, NULL, 4, &detect_task_left);
    //xTaskCreate(peripheral_detect_task, "peripheral_detect_right", 1536, (void*)1, 4, &detect_task_right);
}
