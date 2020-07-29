/**
 * @file hhkbble.c
 */

#include "hhkbble.h"

#define _______ KC_TRNS

const uint8_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0]=LAYOUT_default(
        KC_ESC,     KC_1,     KC_2, KC_3, KC_4, KC_5, KC_6, KC_7,    KC_8,   KC_9,    KC_0, KC_MINS,  KC_EQL, KC_BSLS, KC_GRV,
        KC_TAB,     KC_Q,     KC_W, KC_E, KC_R, KC_T, KC_Y, KC_U,    KC_I,   KC_O,    KC_P, KC_LBRC, KC_RBRC, KC_BSPC,
        KC_LCTRL,   KC_A,     KC_S, KC_D, KC_F, KC_G, KC_H, KC_J,    KC_K,   KC_L, KC_SCLN, KC_QUOT,           KC_ENT,
        KC_LSFT,    KC_Z,     KC_X, KC_C, KC_V, KC_B, KC_N, KC_M, KC_COMM, KC_DOT, KC_SLSH,          KC_RSFT,  KC_END,
                 KC_LGUI,  KC_LALT,                 KC_FN1,                                          KC_RALT,  KC_FN0),
    [1]=LAYOUT_default(
        _______,   KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,  KC_F10,  KC_F11,  KC_F12, _______, _______,
        KC_BTLD, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        KC_CAPS, _______, _______, _______, _______, _______, KC_LEFT, KC_DOWN,   KC_UP,KC_RIGHT, _______, _______,          _______,
        _______,  KC_F21,  KC_F22,  KC_F23, _______, _______, _______, _______, _______, _______, _______,          _______, _______,
                 _______, _______,                   _______,                                                       _______,_______),
};

/*
 * Fn action definition
 */
const action_t PROGMEM fn_actions[] = {
    [0] = ACTION_LAYER_MOMENTARY(1),
    [1] = ACTION_LAYER_TAP_KEY(1, KC_SPC),
};

#if 0
void keyboard_set_rgb(bool on)
{
    if (on) {
        nrf_gpio_pin_set(RGBLIGHT_EN_PIN);
        wait_ms(1);
        aw9523b_init(AW9523B_ADDR);
    } else {
        aw9523b_uninit(AW9523B_ADDR);
        nrf_gpio_pin_clear(RGBLIGHT_EN_PIN);
    }
}

void keyboard_prepare_sleep(void)
{
    // turn off rgb
    keyboard_set_rgb(false);
}

#endif