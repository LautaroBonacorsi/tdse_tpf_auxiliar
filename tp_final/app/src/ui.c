#include "ui.h"
#include "main.h"
#include "max30102.h"

static uint8_t alarm_active = 0;
static uint8_t fault_active = 0;
static uint8_t ack_flag = 0;

static uint32_t ui_tick = 0;

void ui_init(void) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, LD2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
}

void ui_update(void) {
    ui_tick++;
    
    if (fault_active) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
        
        if (ui_tick % 2000 < 1000) {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
        }
        
    } else if (alarm_active) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
        if (ui_tick % 250 < 125) {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
        }
        
        if (ui_tick % 500 < 250) {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
        }
        
    } else {
        if (ui_tick % 1000 < 100) {
            HAL_GPIO_WritePin(GPIOA, LD2_Pin, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(GPIOA, LD2_Pin, GPIO_PIN_RESET);
        }
        
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
    }
}

void ui_set_alarm(uint8_t state) {
    alarm_active = state;
}

void ui_set_fault(uint8_t state) {
    fault_active = state;
}

uint8_t ui_get_ack_flag(void) {
    return ack_flag;
}

void ui_clear_ack_flag(void) {
    ack_flag = 0;
}

void ui_exti_callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == B1_Pin) {
        ack_flag = 1;
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == B1_Pin) {
        ui_exti_callback(GPIO_Pin);
    } else if (GPIO_Pin == GPIO_PIN_4) {
        max30102_exti_callback();
    }
}
