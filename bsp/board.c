/*
 * board.c
 *
 *  Created on: Sep 25, 2026
 *      Author: CemII-2
 */

#include "board.h"
#include "main.h"

extern UART_HandleTypeDef huart2;
extern TIM_HandleTypeDef  htim1;
extern I2C_HandleTypeDef  hi2c1;
extern ADC_HandleTypeDef hadc1;

reset_cause_t board_get_reset_cause(void)
{
    reset_cause_t cause = RESET_CAUSE_UNKNOWN;

    // RCC_CSR holds a flag for the cause of the last reset.
    // PINRST is checked last: internal resets also pull the NRST pin.
    if      (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST)) cause = RESET_CAUSE_LOW_POWER;
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST)) cause = RESET_CAUSE_WWDG;   // refreshed outside its window
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) cause = RESET_CAUSE_IWDG;   // not refreshed in time
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST))  cause = RESET_CAUSE_SOFTWARE;
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST))  cause = RESET_CAUSE_POWER_ON;
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_OBLRST))  cause = RESET_CAUSE_OPTION_BYTE;
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST))  cause = RESET_CAUSE_PIN;

    __HAL_RCC_CLEAR_RESET_FLAGS();
    return cause;
}

const char *board_reset_cause_str(reset_cause_t cause)
{
    switch (cause)
    {
    case RESET_CAUSE_POWER_ON:    return "POWER-ON";
    case RESET_CAUSE_PIN:         return "PIN (reset butonu)";
    case RESET_CAUSE_SOFTWARE:    return "SOFTWARE";
    case RESET_CAUSE_IWDG:        return "IWDG (watchdog!)";
    case RESET_CAUSE_WWDG:        return "WWDG (watchdog!)";
    case RESET_CAUSE_LOW_POWER:   return "LOW-POWER";
    case RESET_CAUSE_OPTION_BYTE: return "OPTION-BYTE";
    default:                      return "UNKNOWN";
    }
}

void board_led_heartbeat_toggle(void)
{
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
}

bool board_button_press(void)
{
    static bool isPressed = false;

    if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)
    {
        if (!isPressed)
        {
            isPressed = true;
            return true;
        }
    }
    else
    {
        isPressed = false;
    }
    return false;
}

bool board_is_i2c_ready(uint8_t address)
{
    return (HAL_I2C_IsDeviceReady(&hi2c1, address << 1, 1, 10) == HAL_OK);
}

void board_pwm_start(void)
{
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
}

void board_pwm_set_duty_percent(uint8_t percent)
{
    if (percent > 100)
    {
        percent = 100;
    }
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim1);
    uint32_t ccr = (percent * (arr + 1)) / 100;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, ccr);
}

void board_motor_set_direction(motor_dir_t dir)
{
    switch (dir)
    {
    case MOTOR_DIR_FORWARD:     // IN1 = 1, IN2 = 0
        HAL_GPIO_WritePin(MOTOR_IN1_GPIO_Port, MOTOR_IN1_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(MOTOR_IN2_GPIO_Port, MOTOR_IN2_Pin, GPIO_PIN_RESET);
        break;

    case MOTOR_DIR_REVERSE:     // IN1 = 0, IN2 = 1
        HAL_GPIO_WritePin(MOTOR_IN1_GPIO_Port, MOTOR_IN1_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_IN2_GPIO_Port, MOTOR_IN2_Pin, GPIO_PIN_SET);
        break;

    case MOTOR_DIR_BRAKE:       // IN1 = 1, IN2 = 1
        HAL_GPIO_WritePin(MOTOR_IN1_GPIO_Port, MOTOR_IN1_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(MOTOR_IN2_GPIO_Port, MOTOR_IN2_Pin, GPIO_PIN_SET);
        break;

    case MOTOR_DIR_STOP:        // IN1 = 0, IN2 = 0
    default:
        HAL_GPIO_WritePin(MOTOR_IN1_GPIO_Port, MOTOR_IN1_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_IN2_GPIO_Port, MOTOR_IN2_Pin, GPIO_PIN_RESET);
        break;
    }
}
void board_adc_init(void)
{
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
}

uint16_t board_pot_read_raw(void)
{
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint16_t raw = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return raw;
}

static volatile uint8_t s_rx_byte;
static volatile bool s_rx_ready = false;

void board_uart_rx_start(void)
{
    HAL_UART_Receive_IT(&huart2, (uint8_t *)&s_rx_byte, 1);

    // İşlemci haber veriyor
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        s_rx_ready = true;
        HAL_UART_Receive_IT(&huart2, (uint8_t *)&s_rx_byte, 1);


    }
}

bool board_uart_get_char(uint8_t *c)
{
    if (!s_rx_ready)
    {
        return false;
    }
    *c = s_rx_byte;
    s_rx_ready = false;
    return true;
}

int __io_putchar(int ch)
{
    uint8_t c = (uint8_t)ch;
    HAL_UART_Transmit(&huart2, &c, 1U, HAL_MAX_DELAY);   // blocking, 1 byte
    return ch;
}
