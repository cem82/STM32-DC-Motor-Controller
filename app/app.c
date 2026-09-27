/*
 * app.c
 *
 *  Created on: Sep 25, 2026
 *      Author: CemII-2
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "app.h"
#include "board.h"
#include "dcmc_config.h"
#include "main.h"
#include "ssd1306.h"

// State

typedef enum {
    STATE_IDLE = 0,
    STATE_RUNNING,
    STATE_STOPPING,
    STATE_FAULT,
} app_state_t;

static app_state_t s_state          = STATE_IDLE;
static uint32_t    s_state_enter_ms = 0;

static motor_dir_t s_set_dir  = MOTOR_DIR_FORWARD;
static uint8_t     s_set_duty = 50;

static motor_dir_t s_dir  = MOTOR_DIR_STOP;
static uint8_t     s_duty = 0;

static uint32_t s_last_heartbeat_ms;
static bool     s_oled_ok = false;

// Names for printing

static const char *dir_name(motor_dir_t dir)
{
    switch (dir)
    {
    case MOTOR_DIR_FORWARD: return "FWD";
    case MOTOR_DIR_REVERSE: return "REV";
    case MOTOR_DIR_STOP:    return "STOP";
    case MOTOR_DIR_BRAKE:   return "BRAKE";
    default:                return "?";
    }
}

static const char *state_name(app_state_t state)
{
    switch (state)
    {
    case STATE_IDLE:     return "IDLE";
    case STATE_RUNNING:  return "RUNNING";
    case STATE_STOPPING: return "STOPPING";
    case STATE_FAULT:    return "FAULT";
    default:             return "?";
    }
}

// Outpust

static void oled_show_status(void)
{
    if (!s_oled_ok)
    {
        return;
    }
    char text[22];

    ssd1306_clear_line(3);
    ssd1306_set_cursor(0, 3);
    ssd1306_write_string(state_name(s_state));

    snprintf(text, sizeof(text), "%s %u%%", dir_name(s_dir), s_duty);
    ssd1306_clear_line(4);
    ssd1306_set_cursor(0, 4);
    ssd1306_write_string(text);

    ssd1306_update();
}

static void motor_apply(motor_dir_t dir, uint8_t duty)
{
    board_pwm_set_duty_percent(0);
    board_motor_set_direction(dir);
    board_pwm_set_duty_percent(duty);

    s_dir  = dir;
    s_duty = duty;
}

// State machine

static void enter_state(app_state_t state)
{
    s_state = state;
    s_state_enter_ms = HAL_GetTick();

    switch (state)
    {
    case STATE_IDLE:
        motor_apply(MOTOR_DIR_STOP, 0);
        break;

    case STATE_RUNNING:
        motor_apply(s_set_dir, s_set_duty);
        break;

    case STATE_STOPPING:
        motor_apply(MOTOR_DIR_BRAKE, 100);
        break;

    case STATE_FAULT:
        motor_apply(MOTOR_DIR_STOP, 0);
        break;
    }

    printf("State: %s\r\n", state_name(state));
    oled_show_status();
}


static void state_update(uint32_t now)
{
    if (s_state == STATE_STOPPING && (now - s_state_enter_ms) >= DCMC_BRAKE_TIME_MS)
    {
        enter_state(STATE_IDLE);
    }
}

static void on_button(void)
{
    switch (s_state)
    {
    case STATE_IDLE:    enter_state(STATE_RUNNING);  break;
    case STATE_RUNNING: enter_state(STATE_STOPPING); break;
    default:
        printf("Button ignored in %s\r\n", state_name(s_state));
        break;
    }
}

// Serial commands

#define LINE_MAX  32

static char    s_line[LINE_MAX];
static uint8_t s_line_len = 0;

static bool read_line(void)
{
    uint8_t c;

    while (board_uart_get_char(&c))
    {
        if (c == '\r' || c == '\n')
        {
            if (s_line_len == 0)
            {
                continue;
            }
            s_line[s_line_len] = '\0';
            s_line_len = 0;
            return true;
        }
        if (s_line_len < LINE_MAX - 1)
        {
            s_line[s_line_len] = (char)c;
            s_line_len++;
        }
    }
    return false;
}

static void process_command(const char *cmd)
{
    if (strcmp(cmd, "HELP") == 0)
    {
        printf("Commands: SET <0-100> | DIR FWD | DIR REV | STOP | BRAKE | STATUS | FAULT | CLEAR\r\n");
    }
    else if (strcmp(cmd, "STATUS") == 0)
    {
        printf("STATE=%s DIR=%s DUTY=%u%% (requested: %s %u%%)\r\n",
               state_name(s_state), dir_name(s_dir), s_duty,
               dir_name(s_set_dir), s_set_duty);
    }
    else if (strcmp(cmd, "STOP") == 0 || strcmp(cmd, "BRAKE") == 0)
    {
        if (s_state == STATE_RUNNING)
        {
            enter_state(STATE_STOPPING);
        }
        printf("OK\r\n");
    }
    else if (strcmp(cmd, "DIR FWD") == 0 || strcmp(cmd, "DIR REV") == 0)
    {
        if (s_state != STATE_IDLE)                 /* never reverse a spinning motor */
        {
            printf("ERR direction can only change in IDLE\r\n");
            return;
        }
        s_set_dir = (cmd[4] == 'F') ? MOTOR_DIR_FORWARD : MOTOR_DIR_REVERSE;
        printf("OK\r\n");
    }
    else if (strncmp(cmd, "SET ", 4) == 0)
    {
        const char *num = cmd + 4;
        char *end;
        long value = strtol(num, &end, 10);

        if (end == num || *end != '\0' || value < 0 || value > 100)
        {
            printf("ERR value must be 0..100\r\n");
            return;
        }
        if (s_state == STATE_STOPPING) {
            printf("Motor is stopping...\r\n");
            return;
        }

        if (s_state == STATE_FAULT) {
            printf("Fault active. Use CLEAR.\r\n");
            return;
        }


        s_set_duty = (uint8_t)value;

        if (s_state == STATE_IDLE)
        {
            enter_state(STATE_RUNNING);
        }
        else
        {
            motor_apply(s_set_dir, s_set_duty);
            oled_show_status();
        }
        printf("OK\r\n");
    }
    else if (strcmp(cmd, "FAULT") == 0)
    {
        enter_state(STATE_FAULT);
        printf("OK\r\n");
    }
    else if (strcmp(cmd, "CLEAR") == 0)
    {
        if (s_state != STATE_FAULT)
        {
            printf("ERR no active fault\r\n");
            return;
        }
        enter_state(STATE_IDLE);
        printf("OK\r\n");
    }
    else
    {
        printf("ERR unknown command (type HELP)\r\n");
    }
}

// Public functions

void app_init(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    reset_cause_t cause = board_get_reset_cause();

    printf("\r\n=== DCMC firmware v%s ===\r\n", DCMC_FW_VERSION);
    printf("SYSCLK : %lu Hz\r\n", SystemCoreClock);
    printf("Reset  : %s\r\n", board_reset_cause_str(cause));

    printf("I2C scan...\r\n");
    for (uint8_t address = 1; address < 128; address++)
    {
        if (board_is_i2c_ready(address))
        {
            printf("  found: 0x%02X\r\n", address);
        }
    }
    printf("I2C scan done\r\n");

    if (ssd1306_init())
    {
        s_oled_ok = true;
        ssd1306_fill(false);
        ssd1306_set_cursor(0, 0);
        ssd1306_write_string("DC Motor Controller");
        ssd1306_set_cursor(0, 1);
        ssd1306_write_string("v" DCMC_FW_VERSION);
        printf("OLED: OK\r\n");
    }
    else
    {
        printf("OLED: init FAILED\r\n");
    }

    board_adc_init();
    board_pwm_start();
    board_uart_rx_start();

    enter_state(STATE_IDLE);
    printf("Ready. Type HELP for commands.\r\n");
}

void app_loop(void)
{
    uint32_t now = HAL_GetTick();

    if ((now - s_last_heartbeat_ms) >= DCMC_HEARTBEAT_PERIOD_MS)
    {
        s_last_heartbeat_ms = now;
        board_led_heartbeat_toggle();
    }

    if (board_button_press())
    {
        on_button();
    }

    if (read_line())
    {
        process_command(s_line);
    }

    state_update(now);
}
