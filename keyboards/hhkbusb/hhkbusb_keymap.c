/*
 * hhkbusb_keymap.c
 */

#include "hhkbusb.h"

#define _______ KC_TRNS
const uint8_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT_default(KC_NO)
};

const action_t fn_actions[] = {
};

const uint32_t keymaps_size = sizeof(keymaps);