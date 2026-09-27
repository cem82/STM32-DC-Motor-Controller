/*
 * board.h
 *
 *  Created on: Sep 25, 2026
 *      Author: CemII-2
 */

#ifndef BOARD_H
#define BOARD_H
#include <stdbool.h>
#include <stdint.h>
typedef enum {
    RESET_CAUSE_UNKNOWN = 0,
    RESET_CAUSE_POWER_ON,
    RESET_CAUSE_PIN,
    RESET_CAUSE_SOFTWARE,
    RESET_CAUSE_IWDG,
    RESET_CAUSE_WWDG,
    RESET_CAUSE_LOW_POWER,
    RESET_CAUSE_OPTION_BYTE,
} reset_cause_t;

typedef enum {
    MOTOR_DIR_STOP = 0,   // IN1 = 0, IN2 = 0
    MOTOR_DIR_FORWARD,    // IN1 = 1, IN2 = 0
    MOTOR_DIR_REVERSE,    // IN1 = 0, IN2 = 1
    MOTOR_DIR_BRAKE,      // IN1 = 1, IN2 = 1
} motor_dir_t;

void board_motor_set_direction(motor_dir_t dir);


reset_cause_t board_get_reset_cause(void); // The reason for the reset
const char   *board_reset_cause_str(reset_cause_t cause); // The reason converts to a strig
void         board_led_heartbeat_toggle(void); // Toggles LED
bool board_button_press(void);
void board_pwm_start(void);
void board_pwm_set_duty_percent(uint8_t percent);
bool board_is_i2c_ready(uint8_t adress);
void board_adc_init(void);
uint16_t board_pot_read_raw(void);
void board_uart_rx_start(void);
bool board_uart_get_char(uint8_t *c);

#endif /* BOARD_H */
