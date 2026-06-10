#include "telemetry.h"
#include "main.h"
#include <string.h>

extern UART_HandleTypeDef huart1;

static uint8_t rx_buffer[1];
static char rx_message[64];
static uint8_t rx_index = 0;

void telemetry_init(void) {
    HAL_UART_Receive_IT(&huart1, rx_buffer, 1);
}

void telemetry_update(void) {
    // Could parse rx_message here outside of ISR context
}

void telemetry_send(const char *msg) {
    HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), 100);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        if (rx_buffer[0] == '\n' || rx_buffer[0] == '\r') {
            rx_message[rx_index] = '\0';
            // Simple command parsing can be added here
            rx_index = 0;
        } else {
            if (rx_index < sizeof(rx_message) - 1) {
                rx_message[rx_index++] = rx_buffer[0];
            }
        }
        HAL_UART_Receive_IT(&huart1, rx_buffer, 1);
    }
}
