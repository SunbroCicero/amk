/**
 * hhkbusb_matrix.c
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "generic_hal.h"
#include "report.h"
#include "matrix.h"
#include "host.h"
#include "gpio_pin.h"
#include "usb_descriptors.h"
#include "amk_printf.h"

extern UART_HandleTypeDef huart1;
pin_t reset_pin = RESET_PIN;

#define QUEUE_ITEM_SIZE   16                        // maximum size of the queue item
typedef struct {
    uint32_t    type;                               // type of the item
    uint8_t     data[QUEUE_ITEM_SIZE];
} report_item_t;

#define QUEUE_SIZE      64
typedef struct {
    report_item_t   items[QUEUE_SIZE];
    uint32_t        head;
    uint32_t        tail;
} report_queue_t;

static report_queue_t report_queue;
//static bool usb_suspended = false;


static bool report_queue_empty(report_queue_t* queue)
{
    return queue->head == queue->tail;
}

static bool report_queue_full(report_queue_t* queue)
{
    return ((queue->tail + 1) % QUEUE_SIZE) == queue->head;
}

static void report_queue_init(report_queue_t* queue)
{
    memset(&queue->items[0], 0, sizeof(queue->items));
    queue->head = queue->tail = 0;
}

static bool report_queue_put(report_queue_t* queue, report_item_t* item)
{
    if (report_queue_full(queue)) return false;

    queue->items[queue->tail] = *item;
    queue->tail = (queue->tail + 1) % QUEUE_SIZE;
    return true;
}

static bool report_queue_get(report_queue_t* queue, report_item_t* item)
{
    if (report_queue_empty(queue)) return false;

    *item = queue->items[queue->head];
    queue->head = (queue->head + 1) % QUEUE_SIZE;
    return true;
}

// uart communication definitions
#define CONFIG_USER_START 0x0800FC00
#define CONFIG_USER_SIZE 0x00000400
#define CONFIG_JUMP_TO_APP_OFFSET 0x09
#define CMD_MAX_LEN 64
#define SYNC_BYTE_1 0xAA
#define SYNC_BYTE_2 0x55
#define RECV_DELAY 100

static void process_data(uint8_t d);
static void enqueue_command(uint8_t *cmd);
static void process_command(report_item_t *item);
static uint8_t compute_checksum(uint8_t *data, uint32_t size);
static void clear_jump_to_app(void);
static void send_set_leds(uint8_t led);

typedef enum
{
    CMD_KEY_REPORT,
    CMD_MOUSE_REPORT,
    CMD_SYSTEM_REPORT,
    CMD_CONSUMER_REPORT,
    CMD_RESET_TO_BOOTLOADER,
    CMD_SET_LEDS,
    CMD_KEYMAP_SET,
    CMD_KEYMAP_SET_ACK,
    CMD_KEYMAP_GET,
    CMD_KEYMAP_GET_ACK,
} command_t;

static uint8_t command_buf[CMD_MAX_LEN];

typedef enum
{
    USER_RESET, // user triggered the reset command
    PIN_RESET, // reset pin was pulled low
} reset_reason_t;

static void reset_to_bootloader(reset_reason_t reason)
{
    if ( reason == USER_RESET) {
        amk_printf("USER reset to bootloader\n");
    } else {
        amk_printf("PIN reset to bootloader\n");
    }
    clear_jump_to_app();

    __set_FAULTMASK(1);
    NVIC_SystemReset();

    // never return
    while (1)
        ;
}

static void process_data(uint8_t d)
{
    //amk_printf("uart received: %d\n", d);
    static uint32_t count = 0;
    if (count == 0 && d != SYNC_BYTE_1) {
        //amk_printf("SYNC BYTE 1: %x\n", d);
        return;
    } else if (count == 1 && d != SYNC_BYTE_2) {
        count = 0;
        //amk_printf("SYNC BYTE 2: %x\n", d);
        return;
    }

    command_buf[count] = d;
    count++;
    if ((count > 2) && (count == (command_buf[2] + 2))) {
        // full packet received
        enqueue_command(&command_buf[2]);
        count = 0;
    }
}

static void enqueue_command(uint8_t *cmd)
{
    amk_printf("Enqueue Command: %d\n", cmd[2]);
    uint8_t checksum = compute_checksum(cmd + 2, cmd[0] - 2);
    if (checksum != cmd[1]) {
        // invalid checksum
        amk_printf("Checksum: LEN:%x, SRC:%x, CUR:%x\n", cmd[0], cmd[1], checksum);
        return;
    }

    report_item_t item;
    item.type = cmd[2];
    memcpy(&item.data[0], &cmd[3], cmd[0]-2);
    report_queue_put(&report_queue, &item);
}

static void process_command(report_item_t *item)
{
    switch (item->type) {
    case CMD_KEY_REPORT: {
        static report_keyboard_t report;
        amk_printf("Send key report\n");
        for (uint32_t i = 0; i < sizeof(report); i++) {
            amk_printf(" 0x%x", item->data[i]);
        }
        amk_printf("\n");

        memcpy(&report.raw[0], &item->data[0], sizeof(report));
        host_keyboard_send(&report);
    } break;
#ifdef MOUSE_ENABLE
    case CMD_MOUSE_REPORT: {
        static report_mouse_t report;
        amk_printf("Send mouse report\n");
        for (uint32_t i = 0; i < sizeof(report); i++) {
            amk_printf(" 0x%x", item->data[i]);
        }

        memcpy(&report, &item->data[0], sizeof(report));
        host_mouse_send(&report);
    } break;
#endif
#ifdef EXTRAKEY_ENABLE
    case CMD_SYSTEM_REPORT: {
        static uint16_t report;
        amk_printf("Send system report\n");
        for (uint32_t i = 0; i < sizeof(report); i++) {
            amk_printf(" 0x%x", item->data[i]);
        }
        memcpy(&report, &item->data[0], sizeof(report));
        host_system_send(report);

    } break;
    case CMD_CONSUMER_REPORT: {
        static uint16_t report;
        amk_printf("Send consumer report\n");
        for (uint32_t i = 0; i < sizeof(report); i++) {
            amk_printf(" 0x%x", item->data[i]);
        }
        memcpy(&report, &item->data[0], sizeof(report));
        host_consumer_send(report);
    } break;
#endif
    case CMD_RESET_TO_BOOTLOADER:
        reset_to_bootloader(USER_RESET);
        break;
    case CMD_KEYMAP_SET_ACK:
        //USBD_COMP_Send(&hUsbDeviceFS, HID_REPORT_ID_WEBUSB, &item->data[0], 6);
        break;
    case CMD_KEYMAP_GET_ACK:
        //USBD_COMP_Send(&hUsbDeviceFS, HID_REPORT_ID_WEBUSB, &item->data[0], 6);
        break;

    default:
        break;
    }
}

static uint8_t compute_checksum(uint8_t *data, uint32_t size)
{
    uint8_t checksum = 0;
    for (uint32_t i = 0; i < size; i++) {
        checksum += data[i];
    }
    return checksum;
}

static void clear_jump_to_app(void)
{
    static uint8_t buf[CONFIG_USER_SIZE];
    uint32_t i = 0;
    uint16_t *pBuf = (uint16_t *)(&(buf[0]));
    uint16_t *pSrc = (uint16_t *)(CONFIG_USER_START);
    for (i = 0; i < CONFIG_USER_SIZE / 2; i++) {
        pBuf[i] = pSrc[i];
    }

    HAL_FLASH_Unlock();
    if (buf[CONFIG_JUMP_TO_APP_OFFSET] != 0) {
        buf[CONFIG_JUMP_TO_APP_OFFSET] = 0;
        FLASH_EraseInitTypeDef erase;
        erase.TypeErase = FLASH_TYPEERASE_PAGES;
        erase.Banks = FLASH_BANK_1;
        erase.PageAddress = CONFIG_USER_START;
        erase.NbPages = 1;
        uint32_t error = 0;
        HAL_FLASHEx_Erase(&erase, &error);
    }

    for (i = 0; i < CONFIG_USER_SIZE / 2; i++) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, (uint32_t)(pSrc + i), pBuf[i]);
    }
    HAL_FLASH_Lock();
}

static void send_set_leds(uint8_t led)
{
    static uint8_t leds_cmd[8];
    leds_cmd[0] = SYNC_BYTE_1;
    leds_cmd[1] = SYNC_BYTE_2;
    leds_cmd[2] = 4;                // size
    leds_cmd[3] = CMD_SET_LEDS+led; // checksum
    leds_cmd[4] = CMD_SET_LEDS;     // command type
    leds_cmd[5] = led;              // led status

    HAL_UART_Transmit_DMA(&huart1, &leds_cmd[0], 6);
}

static matrix_row_t matrix[MATRIX_ROWS];
static uint8_t recv_char;

void matrix_init(void)
{
    report_queue_init(&report_queue);
    HAL_UART_Receive_IT(&huart1, &recv_char, 1);
    gpio_set_input_pullup(reset_pin);
}

uint8_t matrix_scan(void)
{
    if (gpio_read_pin(reset_pin) == 0) {
        reset_to_bootloader(PIN_RESET);
    }

    report_item_t item;
    while (report_queue_get(&report_queue, &item)) {
        process_command(&item);
    }
    return 1;
}

matrix_row_t matrix_get_row(uint8_t row) { return matrix[row]; }

void matrix_print(void)
{
    amk_printf("matrix_print\n");
}

void led_set(uint8_t usb_led)
{
    send_set_leds(usb_led);
}

void uart_recv_char(uint8_t c)
{
    process_data(c);
}

void uart_keymap_set(uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode)
{
    static uint8_t set_cmd[16];
    set_cmd[0] = SYNC_BYTE_1;
    set_cmd[1] = SYNC_BYTE_2;
    set_cmd[2] = 8;                 // size
    set_cmd[4] = CMD_KEYMAP_SET;    // command type
    set_cmd[5] = layer;             
    set_cmd[6] = row;
    set_cmd[7] = col;
    set_cmd[8] = ((keycode>>8)&0xFF);
    set_cmd[9] = (keycode&0xFF);

    set_cmd[3] = CMD_KEYMAP_SET+layer+row+col+set_cmd[8]+set_cmd[9]; // checksum

    HAL_UART_Transmit_DMA(&huart1, &set_cmd[0], 10);
}

void uart_keymap_get(uint8_t layer, uint8_t row, uint8_t col)
{
    static uint8_t get_cmd[16];
    get_cmd[0] = SYNC_BYTE_1;
    get_cmd[1] = SYNC_BYTE_2;
    get_cmd[2] = 8;                 // size
    get_cmd[4] = CMD_KEYMAP_GET;    // command type
    get_cmd[5] = layer;             
    get_cmd[6] = row;
    get_cmd[7] = col;

    get_cmd[3] = CMD_KEYMAP_GET+layer+row+col; // checksum

    HAL_UART_Transmit_DMA(&huart1, &get_cmd[0], 8);
}