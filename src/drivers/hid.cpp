#include "hid.hpp"
#include <tusb.h>
#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>
#include <pico/time.h>
#include "../event.hpp"

/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

// If your host terminal support ansi escape code such as TeraTerm
// it can be use to simulate mouse cursor movement within terminal
#define USE_ANSI_ESCAPE   0

#define MAX_REPORT  4

static uint8_t const keycode2ascii[128][2] = {
    {0     , 0      }, /* 0x00 */
    {0     , 0      }, /* 0x01 */
    {0     , 0      }, /* 0x02 */
    {0     , 0      }, /* 0x03 */
    {'a'   , 'A'    }, /* 0x04 */
    {'b'   , 'B'    }, /* 0x05 */
    {'c'   , 'C'    }, /* 0x06 */
    {'d'   , 'D'    }, /* 0x07 */
    {'e'   , 'E'    }, /* 0x08 */
    {'f'   , 'F'    }, /* 0x09 */
    {'g'   , 'G'    }, /* 0x0a */
    {'h'   , 'H'    }, /* 0x0b */
    {'i'   , 'I'    }, /* 0x0c */
    {'j'   , 'J'    }, /* 0x0d */
    {'k'   , 'K'    }, /* 0x0e */
    {'l'   , 'L'    }, /* 0x0f */
    {'m'   , 'M'    }, /* 0x10 */
    {'n'   , 'N'    }, /* 0x11 */
    {'o'   , 'O'    }, /* 0x12 */
    {'p'   , 'P'    }, /* 0x13 */
    {'q'   , 'Q'    }, /* 0x14 */
    {'r'   , 'R'    }, /* 0x15 */
    {'s'   , 'S'    }, /* 0x16 */
    {'t'   , 'T'    }, /* 0x17 */
    {'u'   , 'U'    }, /* 0x18 */
    {'v'   , 'V'    }, /* 0x19 */
    {'w'   , 'W'    }, /* 0x1a */
    {'x'   , 'X'    }, /* 0x1b */
    {'y'   , 'Y'    }, /* 0x1c */
    {'z'   , 'Z'    }, /* 0x1d */
    {'1'   , '!'    }, /* 0x1e */
    {'2'   , '@'    }, /* 0x1f */
    {'3'   , '#'    }, /* 0x20 */
    {'4'   , '$'    }, /* 0x21 */
    {'5'   , '%'    }, /* 0x22 */
    {'6'   , '^'    }, /* 0x23 */
    {'7'   , '&'    }, /* 0x24 */
    {'8'   , '*'    }, /* 0x25 */
    {'9'   , '('    }, /* 0x26 */
    {'0'   , ')'    }, /* 0x27 */
    {0     , 0      }, /* 0x28 */
    {0     , 0      }, /* 0x29 */
    {0     , 0      }, /* 0x2a */
    {0     , 0      }, /* 0x2b */
    {' '   , ' '    }, /* 0x2c */
    {'-'   , '_'    }, /* 0x2d */
    {'='   , '+'    }, /* 0x2e */
    {'['   , '{'    }, /* 0x2f */
    {']'   , '}'    }, /* 0x30 */
    {'\\'  , '|'    }, /* 0x31 */
    {'#'   , '~'    }, /* 0x32 */
    {';'   , ':'    }, /* 0x33 */
    {'\''  , '\"'   }, /* 0x34 */
    {'`'   , '~'    }, /* 0x35 */
    {','   , '<'    }, /* 0x36 */
    {'.'   , '>'    }, /* 0x37 */
    {'/'   , '?'    }, /* 0x38 */
                                
    {0     , 0      }, /* 0x39 */
    {0     , 0      }, /* 0x3a */
    {0     , 0      }, /* 0x3b */
    {0     , 0      }, /* 0x3c */
    {0     , 0      }, /* 0x3d */
    {0     , 0      }, /* 0x3e */
    {0     , 0      }, /* 0x3f */
    {0     , 0      }, /* 0x40 */
    {0     , 0      }, /* 0x41 */
    {0     , 0      }, /* 0x42 */
    {0     , 0      }, /* 0x43 */
    {0     , 0      }, /* 0x44 */
    {0     , 0      }, /* 0x45 */
    {0     , 0      }, /* 0x46 */
    {0     , 0      }, /* 0x47 */
    {0     , 0      }, /* 0x48 */
    {0     , 0      }, /* 0x49 */
    {0     , 0      }, /* 0x4a */
    {0     , 0      }, /* 0x4b */
    {0     , 0      }, /* 0x4c */
    {0     , 0      }, /* 0x4d */
    {0     , 0      }, /* 0x4e */
    {0     , 0      }, /* 0x4f */
    {0     , 0      }, /* 0x50 */
    {0     , 0      }, /* 0x51 */
    {0     , 0      }, /* 0x52 */
    {0     , 0      }, /* 0x53 */
                                
    {'/'   , '/'    }, /* 0x54 */
    {'*'   , '*'    }, /* 0x55 */
    {'-'   , '-'    }, /* 0x56 */
    {'+'   , '+'    }, /* 0x57 */
    {0     , 0      }, /* 0x58 */
    {'1'   , 0      }, /* 0x59 */
    {'2'   , 0      }, /* 0x5a */
    {'3'   , 0      }, /* 0x5b */
    {'4'   , 0      }, /* 0x5c */
    {'5'   , '5'    }, /* 0x5d */
    {'6'   , 0      }, /* 0x5e */
    {'7'   , 0      }, /* 0x5f */
    {'8'   , 0      }, /* 0x60 */
    {'9'   , 0      }, /* 0x61 */
    {'0'   , 0      }, /* 0x62 */
    {'.'   , 0      }, /* 0x63 */
    {0     , 0      }, /* 0x64 */
    {0     , 0      }, /* 0x65 */
    {0     , 0      }, /* 0x66 */
    {'='   , '='    }, /* 0x67 */
};

// Each HID instance can has multiple reports
static struct {
  uint8_t report_count;
  tuh_hid_report_info_t report_info[MAX_REPORT];
} hid_info[CFG_TUH_HID];

static void process_kbd_report(hid_keyboard_report_t const *report);
static void process_mouse_report(hid_mouse_report_t const *report);
static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len);

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len) {
  printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);

  // Interface protocol (hid_interface_protocol_enum_t)
  const char *protocol_str[] = { "None", "Keyboard", "Mouse" };
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  printf("HID Interface Protocol = %s\r\n", protocol_str[itf_protocol]);

  // By default host stack will use activate boot protocol on supported interface.
  // Therefore for this simple example, we only need to parse generic report descriptor (with built-in parser)
  if (itf_protocol == HID_ITF_PROTOCOL_NONE) {
    hid_info[instance].report_count = tuh_hid_parse_report_descriptor(hid_info[instance].report_info, MAX_REPORT,
                                                                      desc_report, desc_len);
    printf("HID has %u reports \r\n", hid_info[instance].report_count);
  }

  // request to receive report
  // tuh_hid_report_received_cb() will be invoked when report is available
  if (!tuh_hid_receive_report(dev_addr, instance)) {
    printf("Error: cannot request to receive report\r\n");
  }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len) {
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  switch (itf_protocol) {
    case HID_ITF_PROTOCOL_KEYBOARD:
      TU_LOG2("HID receive boot keyboard report\r\n");
      process_kbd_report((hid_keyboard_report_t const *) report);
      break;

    case HID_ITF_PROTOCOL_MOUSE:
      TU_LOG2("HID receive boot mouse report\r\n");
      process_mouse_report((hid_mouse_report_t const *) report);
      break;

    default:
      // Generic report requires matching ReportID and contents with previous parsed report info
      process_generic_report(dev_addr, instance, report, len);
      break;
  }

  // continue to request to receive report
  if (!tuh_hid_receive_report(dev_addr, instance)) {
    printf("Error: cannot request to receive report\r\n");
  }
}

//--------------------------------------------------------------------+
// Keyboard
//--------------------------------------------------------------------+

static const uint8_t cc_keymap[256] = {
    0, 0, 0, 0, 30, 48, 46, 32,                 // 00
    18, 33, 34, 35, 23, 36, 37, 38,             // 08
    50, 49, 24, 25, 16, 19, 31, 20,             // 10
    22, 47, 17, 45, 21, 44, 2, 3,               // 18
    4, 5, 6, 7, 8, 9, 10, 11,                   // 20
    28, 1, 14, 15, 57, 12, 13, 26,              // 28
    27, 43, 0, 39, 40, 41, 51, 52,              // 30
    43, 58, 59, 60, 61, 62, 63, 64,             // 38
    65, 66, 67, 68, 87, 88, 196, 70,            // 40
    197, 198, 199, 201, 211, 207, 209, 205,     // 48
    203, 208, 200, 69, 181, 55, 74, 78,         // 50
    156, 79, 80, 81, 75, 76, 77, 71,            // 58
    72, 73, 82, 83, 43, 0, 0, 0,                // 60
    0, 0, 0, 0, 0, 0, 0, 0,                     // 68
    0, 0, 0, 0, 0, 0, 0, 0,                     // 70
    0, 0, 0, 0, 0, 0, 0, 0,                     // 78
    0, 0, 0, 0, 0, 179, 141, 0,                 // 80
    112, 125, 121, 123, 0, 0, 0, 0,             // 88
    0, 0, 0, 0, 0, 0, 0, 0,                     // 90
    0, 0, 0, 0, 0, 0, 0, 0,                     // 98
    0, 0, 0, 0, 0, 0, 0, 0,                     // A0
    0, 0, 0, 0, 0, 0, 0, 0,                     // A8
    0, 0, 0, 0, 0, 0, 0, 0,                     // B0
    0, 0, 0, 0, 0, 0, 0, 0,                     // B8
    0, 0, 0, 0, 0, 0, 0, 0,                     // C0
    0, 0, 0, 0, 0, 0, 0, 0,                     // C8
    0, 0, 0, 0, 0, 0, 0, 0,                     // D0
    0, 0, 0, 0, 0, 0, 0, 0,                     // D8
    29, 42, 56, 91, 157, 54, 184, 92,           // E0
    0, 0, 0, 0, 0, 0, 0, 0,                     // E8
    0, 0, 0, 0, 0, 0, 0, 0,                     // F0
    0, 0, 0, 0, 0, 0, 0, 0,                     // F8
};
static const uint8_t cc_modifiers[8] = {29, 42, 56, 91, 157, 54, 184, 92};
static const char *hid_proto_name_str[] = {
    "NONE",
    "KEYBOARD",
    "MOUSE"
};

static absolute_time_t keypress_time[256] = {0};
static hid_keyboard_report_t prev_report = {0, 0, {0}};
static bool shift_held = false, ctrl_held = false, caps_lock = false;

static void key_cb(uint8_t key, bool isHeld) {
    event_t event;
    event.type = EVENT_TYPE_KEY;
    event.key.keycode = key;
    event.key.repeat = isHeld;
    event_push(&event);
}

static void keyUp_cb(uint8_t key) {
    event_t event;
    event.type = EVENT_TYPE_KEY_UP;
    event.key.keycode = key;
    event_push(&event);
}

static void char_cb(char c) {
    event_t event;
    event.type = EVENT_TYPE_CHAR;
    event.character.c = c;
    event_push(&event);
}

static void repeat_timer(TimerHandle_t timer) {
    absolute_time_t t = get_absolute_time();
    if (ctrl_held) {
        if (keypress_time[0x15] && absolute_time_diff_us(t, keypress_time[0x15]) > 600000) {
            // TODO: reset
        }
        if (keypress_time[0x17] && absolute_time_diff_us(t, keypress_time[0x17]) > 600000) {
            event_t event;
            event.type = EVENT_TYPE_TERMINATE;
            event_push(&event);
            keypress_time[0x17] = 0;
        }
    }
    for (int key = 0; key < 256; key++) {
        if (keypress_time[key] && cc_keymap[key] && absolute_time_diff_us(t, keypress_time[key]) > 600000) {
            key_cb(cc_keymap[key], true);
            if (keycode2ascii[key][0])
                char_cb(keycode2ascii[key][(shift_held != caps_lock) ? 1 : 0]);
        }
    }
}

// look up new key in previous keys
static inline bool find_key_in_report(hid_keyboard_report_t const *report, uint8_t keycode) {
  for (uint8_t i = 0; i < 6; i++) {
    if (report->keycode[i] == keycode) return true;
  }

  return false;
}

static void process_kbd_report(hid_keyboard_report_t const *report) {
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t key = report->keycode[i];
        if (key && !keypress_time[key]) {
            if (cc_keymap[key]) {
                key_cb(cc_keymap[key], false);
                if (keycode2ascii[key][0])
                    char_cb(keycode2ascii[key][((report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) != caps_lock) ? 1 : 0]);
            }
            if (key == 0x39) { // caps lock
                caps_lock = !caps_lock;
                uint8_t rp = caps_lock ? 2 : 0;
                //hid_class_request_set_report(hid_device_handle, HID_REPORT_TYPE_OUTPUT, HID_REPORT_TYPE_OUTPUT, &rp, 1);
            }
            keypress_time[key] = get_absolute_time();
        }
    }

    uint8_t mods = report->modifier ^ prev_report.modifier;
    for (uint8_t i = 0; i < 8; i++) {
        if (mods & (1 << i)) {
            if (report->modifier & (1 << i)) key_cb(cc_modifiers[i], false);
            else keyUp_cb(cc_modifiers[i]);
        }
    }
    shift_held = report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
    ctrl_held = report->modifier & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL);

    for (uint8_t i = 0; i < 6; i++) {
        uint8_t key = prev_report.keycode[i];
        if (key && !find_key_in_report(report, key)) {
            if (cc_keymap[key]) keyUp_cb(cc_keymap[key]);
            keypress_time[key] = 0;
        }
    }

    prev_report = *report;
}

//--------------------------------------------------------------------+
// Mouse
//--------------------------------------------------------------------+

void cursor_movement(int8_t x, int8_t y, int8_t wheel) {
#if USE_ANSI_ESCAPE
  // Move X using ansi escape
  if ( x < 0) {
    printf(ANSI_CURSOR_BACKWARD(%d), (-x)); // move left
  }else if ( x > 0) {
    printf(ANSI_CURSOR_FORWARD(%d), x); // move right
  }

  // Move Y using ansi escape
  if ( y < 0) {
    printf(ANSI_CURSOR_UP(%d), (-y)); // move up
  }else if ( y > 0) {
    printf(ANSI_CURSOR_DOWN(%d), y); // move down
  }

  // Scroll using ansi escape
  if (wheel < 0) {
    printf(ANSI_SCROLL_UP(%d), (-wheel)); // scroll up
  }else if (wheel > 0) {
    printf(ANSI_SCROLL_DOWN(%d), wheel); // scroll down
  }

  printf("\r\n");
#else
  printf("(%d %d %d)\r\n", x, y, wheel);
#endif
}

static void process_mouse_report(hid_mouse_report_t const *report) {
  static hid_mouse_report_t prev_report = { 0 };

  //------------- button state  -------------//
  uint8_t button_changed_mask = report->buttons ^ prev_report.buttons;
  if (button_changed_mask & report->buttons) {
    printf(" %c%c%c ",
           report->buttons & MOUSE_BUTTON_LEFT ? 'L' : '-',
           report->buttons & MOUSE_BUTTON_MIDDLE ? 'M' : '-',
           report->buttons & MOUSE_BUTTON_RIGHT ? 'R' : '-');
  }

  //------------- cursor movement -------------//
  cursor_movement(report->x, report->y, report->wheel);
}

//--------------------------------------------------------------------+
// Generic Report
//--------------------------------------------------------------------+
static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len) {
  (void) dev_addr;

  uint8_t const rpt_count = hid_info[instance].report_count;
  tuh_hid_report_info_t *rpt_info_arr = hid_info[instance].report_info;
  tuh_hid_report_info_t *rpt_info = NULL;

  if (rpt_count == 1 && rpt_info_arr[0].report_id == 0) {
    // Simple report without report ID as 1st byte
    rpt_info = &rpt_info_arr[0];
  } else {
    // Composite report, 1st byte is report ID, data starts from 2nd byte
    uint8_t const rpt_id = report[0];

    // Find report id in the array
    for (uint8_t i = 0; i < rpt_count; i++) {
      if (rpt_id == rpt_info_arr[i].report_id) {
        rpt_info = &rpt_info_arr[i];
        break;
      }
    }

    report++;
    len--;
  }

  if (!rpt_info) {
    printf("Couldn't find report info !\r\n");
    return;
  }

  // For complete list of Usage Page & Usage checkout src/class/hid/hid.h. For examples:
  // - Keyboard                     : Desktop, Keyboard
  // - Mouse                        : Desktop, Mouse
  // - Gamepad                      : Desktop, Gamepad
  // - Consumer Control (Media Key) : Consumer, Consumer Control
  // - System Control (Power key)   : Desktop, System Control
  // - Generic (vendor)             : 0xFFxx, xx
  if (rpt_info->usage_page == HID_USAGE_PAGE_DESKTOP) {
    switch (rpt_info->usage) {
      case HID_USAGE_DESKTOP_KEYBOARD:
        TU_LOG1("HID receive keyboard report\r\n");
        // Assume keyboard follow boot report layout
        process_kbd_report((hid_keyboard_report_t const *) report);
        break;

      case HID_USAGE_DESKTOP_MOUSE:
        TU_LOG1("HID receive mouse report\r\n");
        // Assume mouse follow boot report layout
        process_mouse_report((hid_mouse_report_t const *) report);
        break;

      default:
        break;
    }
  }
}

// USB Host task
// This top level thread process all usb events and invoke callbacks
static void usb_host_task(void *param) {
  (void) param;

  // init host stack on configured roothub port
  if (!tuh_init(BOARD_TUH_RHPORT)) {
    printf("Failed to init USB Host Stack\r\n");
    vTaskSuspend(NULL);
  }

#if CFG_TUH_ENABLED && CFG_TUH_MAX3421
  // FeatherWing MAX3421E use MAX3421E's GPIO0 for VBUS enable
  enum { IOPINS1_ADDR  = 20u << 3, /* 0xA0 */ };
  tuh_max3421_reg_write(BOARD_TUH_RHPORT, IOPINS1_ADDR, 0x01, false);
#endif

  // RTOS forever loop
  while (1) {
    // put this thread to waiting state until there is new events
    tuh_task();

    // following code only run if tuh_task() process at least 1 event
    vTaskDelay(1);
  }
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

void tuh_mount_cb(uint8_t dev_addr) {
  // application set-up
  printf("A device with address %d is mounted\r\n", dev_addr);
}

void tuh_umount_cb(uint8_t dev_addr) {
  // application tear-down
  printf("A device with address %d is unmounted \r\n", dev_addr);
}

static TimerHandle_t repeat_tm;

void hid_init() {
    repeat_tm = xTimerCreate(NULL, pdMS_TO_TICKS(100), true, NULL, repeat_timer);
    xTimerStart(repeat_tm, portMAX_DELAY);
    xTaskCreate(usb_host_task, "usbd", 4096, NULL, configMAX_PRIORITIES-1, NULL);
}

void hid_deinit() {

}