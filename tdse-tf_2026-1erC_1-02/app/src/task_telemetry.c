#include "main.h"
#include "app.h"
#include "task_telemetry.h"
#include "task_telemetry_attribute.h"
#include "task_telemetry_interface.h"
#include "task_system.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart1;
extern uint32_t algo_current_spo2;
extern uint32_t algo_current_bpm;

task_telemetry_dta_t task_telemetry_dta_list[] = {
    {0, ST_TEL_IDLE, EV_TEL_IDLE, false}
};

task_telemetry_dta_t *p_task_telemetry_dta;

#define TX_BUFFER_SIZE 64
#define RX_BUFFER_SIZE 32

static char tx_buffer[TX_BUFFER_SIZE];
static char rx_buffer[RX_BUFFER_SIZE];
static uint8_t rx_byte;
static uint8_t rx_index = 0;

void task_telemetry_init(void *parameters)
{
    p_task_telemetry_dta = &task_telemetry_dta_list[0];
    p_task_telemetry_dta->state = ST_TEL_IDLE;
    p_task_telemetry_dta->event = EV_TEL_IDLE;
    
    init_event_task_telemetry();
    
    // Iniciar recepción por interrupción (1 byte)
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

void task_telemetry_update(void *parameters)
{
    if (any_event_task_telemetry()) {
        p_task_telemetry_dta->flag = true;
        p_task_telemetry_dta->event = get_event_task_telemetry();
    }

    switch (p_task_telemetry_dta->state) {
        case ST_TEL_IDLE:
            if (p_task_telemetry_dta->flag) {
                p_task_telemetry_dta->flag = false;
                
                if (p_task_telemetry_dta->event == EV_TEL_SPO2_DATA) {
                    bool alarm = task_system_is_alarm_active();
                    snprintf(tx_buffer, TX_BUFFER_SIZE, "{\"type\":\"data\",\"spo2\":%lu,\"bpm\":%lu,\"alarm\":%d}\n", 
                             algo_current_spo2, algo_current_bpm, alarm ? 1 : 0);
                             
                    p_task_telemetry_dta->state = ST_TEL_TX;
                    HAL_UART_Transmit_DMA(&huart1, (uint8_t*)tx_buffer, strlen(tx_buffer));
                }
                else if (p_task_telemetry_dta->event == EV_TEL_SENSOR_ERR) {
                    snprintf(tx_buffer, TX_BUFFER_SIZE, "{\"type\":\"error\",\"msg\":\"NO_FINGER\"}\n");
                    
                    p_task_telemetry_dta->state = ST_TEL_TX;
                    HAL_UART_Transmit_DMA(&huart1, (uint8_t*)tx_buffer, strlen(tx_buffer));
                }
                else if (p_task_telemetry_dta->event == EV_TEL_RX_DATA) {
                    // Parse incoming command
                    // Example: CFG:SPO2_MIN:90
                    // CFG:BPM_MIN:50
                    // CFG:BPM_MAX:120
                    // CFG:ALARM:1
                    
                    char param[16];
                    int val;
                    if (sscanf(rx_buffer, "CFG:%15[^:]:%d", param, &val) == 2) {
                        int32_t spo2_min = -1, bpm_min = -1, bpm_max = -1;
                        int8_t alarm_en = -1;
                        
                        if (strcmp(param, "SPO2_MIN") == 0) spo2_min = val;
                        else if (strcmp(param, "BPM_MIN") == 0) bpm_min = val;
                        else if (strcmp(param, "BPM_MAX") == 0) bpm_max = val;
                        else if (strcmp(param, "ALARM") == 0) alarm_en = val;
                        
                        task_system_update_config_from_telemetry(spo2_min, bpm_min, bpm_max, alarm_en);
                    }
                }
            }
            break;
            
        case ST_TEL_TX:
            if (p_task_telemetry_dta->flag) {
                p_task_telemetry_dta->flag = false;
                if (p_task_telemetry_dta->event == EV_TEL_TX_CPLT) {
                    p_task_telemetry_dta->state = ST_TEL_IDLE;
                }
                // Si llegan datos (RX) o de sensores mientras transmite, 
                // se encolan automáticamente porque el driver I2C/UART es asíncrono
                else if (p_task_telemetry_dta->event != EV_TEL_TX_CPLT) {
                    // Re-encolar el evento para procesarlo cuando vuelva a IDLE
                    put_event_task_telemetry(p_task_telemetry_dta->event);
                }
            }
            break;
            
        default:
            p_task_telemetry_dta->state = ST_TEL_IDLE;
            break;
    }
}

// ISR Callbacks a enlazar desde app.c
void telemetry_rx_cplt_callback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        if (rx_byte == '\n' || rx_byte == '\r') {
            if (rx_index > 0) {
                rx_buffer[rx_index] = '\0';
                put_event_task_telemetry(EV_TEL_RX_DATA);
                rx_index = 0;
            }
        } else {
            if (rx_index < RX_BUFFER_SIZE - 1) {
                rx_buffer[rx_index++] = rx_byte;
            }
        }
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}

void telemetry_tx_cplt_callback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        put_event_task_telemetry(EV_TEL_TX_CPLT);
    }
}
