#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <hardware/irq.h>
#include <hardware/pio.h>
#include <hardware/clocks.h>
#include <pico/time.h>
#include <craftos.h>
#include "../event.hpp"
#include "peripheral.hpp"

extern "C" {
    extern craftos_machine_t machine;
}

SemaphoreHandle_t onewire_lock[2];
OW onewire_inst[2];

static TaskHandle_t detect_task_left, detect_task_right;
static struct peripheral * peripherals[8] = {
    NULL,
    &peripheral_drive,
    &peripheral_speaker,
    &peripheral_modem,
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
    const uint led_pin = isRight ? 36 : 7;
    OW *ow_inst = &onewire_inst[(int)isRight];
    SemaphoreHandle_t lock = onewire_lock[(int)isRight];

    // Choose PIO instance (0 or 1)
    PIO pio = isRight ? pio1 : pio0;

    // Get first free state machine in PIO 0
    uint sm = pio_claim_unused_sm(pio, true);

    // Add PIO program to PIO instruction memory. SDK will find location and
    // return with the memory offset of the program.
    uint offset = pio_add_program(pio, &onewire_program);

    ow_init(ow_inst, pio, offset, led_pin);

    int last_id = -1;
    void* userp = NULL;
    while (true) {
        xSemaphoreTake(lock, portMAX_DELAY);
        if (ow_reset(ow_inst)) {
            ow_send(ow_inst, PERIPHERAL_CMD_GET_TYPE);
            uint8_t id = ow_read(ow_inst);
            if (last_id != -1 && last_id != id && peripherals[last_id] != NULL && userp != NULL) {
                peripherals[last_id]->close(userp);
                event_t event;
                event.type = EVENT_TYPE_PERIPHERAL_DETACH;
                event.peripheral.side = isRight ? "right" : "left";
                event_push(&event);
                craftos_machine_peripheral_detach(machine, isRight ? "right" : "left");
                last_id = -1;
            }
            if (last_id == -1 && id < 8) {
                if (peripherals[id] != NULL) {
                    if (peripherals[id]->open(isRight, &userp) == 0) {
                        event_t event;
                        event.type = EVENT_TYPE_PERIPHERAL;
                        event.peripheral.side = isRight ? "right" : "left";
                        event_push(&event);
                        craftos_machine_peripheral_attach(machine, isRight ? "right" : "left", peripherals[id]->types, peripherals[id]->funcs, userp);
                    }
                }
                last_id = id;
            }
        } else if (last_id != -1) {
            peripherals[last_id]->close(userp);
            event_t event;
            event.type = EVENT_TYPE_PERIPHERAL_DETACH;
            event.peripheral.side = isRight ? "right" : "left";
            event_push(&event);
            craftos_machine_peripheral_detach(machine, isRight ? "right" : "left");
            last_id = -1;
        }
        xSemaphoreGive(lock);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void peripheral_detect_init() {
    onewire_lock[0] = xSemaphoreCreateMutex();
    onewire_lock[1] = xSemaphoreCreateMutex();
    xTaskCreate(peripheral_detect_task, "peripheral_detect_left", 1536, NULL, 4, &detect_task_left);
    xTaskCreate(peripheral_detect_task, "peripheral_detect_right", 1536, (void*)1, 4, &detect_task_right);
    gpio_set_function(32, GPIO_FUNC_NULL);
    gpio_set_function(33, GPIO_FUNC_NULL);
    gpio_set_function(34, GPIO_FUNC_NULL);
    gpio_set_function(35, GPIO_FUNC_NULL);
    gpio_set_function(8, GPIO_FUNC_NULL);
    gpio_set_function(9, GPIO_FUNC_NULL);
    gpio_set_function(10, GPIO_FUNC_NULL);
    gpio_set_function(11, GPIO_FUNC_NULL);
}
