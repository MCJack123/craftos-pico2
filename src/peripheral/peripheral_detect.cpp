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
#include "../drivers/1wire.h"

extern "C" {
    extern craftos_machine_t machine;
}

static SemaphoreHandle_t onewire_lock;
static OW onewire_inst[2];
static TaskHandle_t detect_task;
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

int peripheral_detect_query(bool right, uint8_t cmd) {
    int retval = -1;
    OW *ow_inst = &onewire_inst[right ? 1 : 0];
    xSemaphoreTake(onewire_lock, portMAX_DELAY);
    if (ow_reset(ow_inst)) {
        ow_send(ow_inst, cmd);
        retval = ow_read(ow_inst);
    }
    xSemaphoreGive(onewire_lock);
    return retval;
}

static void peripheral_detect_task(void* unused) {
    // Choose PIO instance (0 or 1)
    PIO pio = pio2;

    // Add PIO program to PIO instruction memory. SDK will find location and
    // return with the memory offset of the program.
    uint offset = pio_add_program(pio, &onewire_program);

    ow_init(&onewire_inst[0], pio, offset, 7);  // left
    ow_init(&onewire_inst[1], pio, offset, 36); // right
    while (machine == NULL) vTaskDelay(1);

    int last_id[2] = {-1, -1};
    void* userp[2] = {NULL, NULL};
    while (true) {
        for (int isRight = 0; isRight < 2; isRight++) {
            int id = peripheral_detect_query(isRight, PERIPHERAL_CMD_GET_TYPE);
            for (int i = 0; i < 5; i++) if (peripheral_detect_query(isRight, PERIPHERAL_CMD_GET_TYPE) != id) goto skip;
            if (id != -1) {
                if (last_id[isRight] != -1 && last_id[isRight] != id && peripherals[last_id[isRight]] != NULL && userp[isRight] != NULL) {
                    peripherals[last_id[isRight]]->close(userp[isRight]);
                    event_t event;
                    event.type = EVENT_TYPE_PERIPHERAL_DETACH;
                    event.peripheral.side = isRight ? "right" : "left";
                    event_push(&event);
                    craftos_machine_peripheral_detach(machine, isRight ? "right" : "left");
                    last_id[isRight] = -1;
                }
                if (last_id[isRight] == -1 && id < 8) {
                    if (peripherals[id] != NULL) {
                        if (peripherals[id]->open(isRight, &userp[isRight]) == 0) {
                            event_t event;
                            event.type = EVENT_TYPE_PERIPHERAL;
                            event.peripheral.side = isRight ? "right" : "left";
                            event_push(&event);
                            craftos_machine_peripheral_attach(machine, isRight ? "right" : "left", peripherals[id]->types, peripherals[id]->funcs, userp[isRight]);
                        }
                    }
                    last_id[isRight] = id;
                }
            } else if (last_id[isRight] != -1 && peripherals[last_id[isRight]] != NULL) {
                peripherals[last_id[isRight]]->close(userp[isRight]);
                event_t event;
                event.type = EVENT_TYPE_PERIPHERAL_DETACH;
                event.peripheral.side = isRight ? "right" : "left";
                event_push(&event);
                craftos_machine_peripheral_detach(machine, isRight ? "right" : "left");
                last_id[isRight] = -1;
            }
            skip: (void)0;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void peripheral_detect_init() {
    onewire_lock = xSemaphoreCreateMutex();
    vQueueAddToRegistry(onewire_lock, "onewire_lock");
    xTaskCreate(peripheral_detect_task, "peripheral_detect", 1536, NULL, 4, &detect_task);
    gpio_set_function(32, GPIO_FUNC_NULL);
    gpio_set_function(33, GPIO_FUNC_NULL);
    gpio_set_function(34, GPIO_FUNC_NULL);
    gpio_set_function(35, GPIO_FUNC_NULL);
    gpio_set_function(8, GPIO_FUNC_NULL);
    gpio_set_function(9, GPIO_FUNC_NULL);
    gpio_set_function(10, GPIO_FUNC_NULL);
    gpio_set_function(11, GPIO_FUNC_NULL);
}
