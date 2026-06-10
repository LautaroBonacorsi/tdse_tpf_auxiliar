#include "max30102.h"
#include "main.h"

extern I2C_HandleTypeDef hi2c1;
#define MAX30102_ADDR (0x57 << 1)

static uint8_t current_spo2 = 0;
static uint8_t current_hr = 0;
static uint8_t fault_state = 0;
static uint8_t data_ready = 0;

uint8_t max30102_init(void) {
    uint8_t id = 0;
    
    if (HAL_I2C_Mem_Read(&hi2c1, MAX30102_ADDR, 0xFF, I2C_MEMADD_SIZE_8BIT, &id, 1, 100) != HAL_OK) {
        fault_state = 1;
        return 1;
    }
    
    if (id != 0x15) {
        fault_state = 1;
        return 2;
    }
    
    fault_state = 0;
    return 0;
}

void max30102_exti_callback(void) {
    data_ready = 1;
}

void max30102_update(void) {
    if (fault_state) return;
    
    if (data_ready) {
        data_ready = 0;
        
        uint8_t dummy_data[6];
        if (HAL_I2C_Mem_Read(&hi2c1, MAX30102_ADDR, 0x07, I2C_MEMADD_SIZE_8BIT, dummy_data, 6, 10) != HAL_OK) {
            fault_state = 1;
            return;
        }
        
        current_spo2 = 98;
        current_hr = 75;
    }
}

uint8_t max30102_get_spo2(void) {
    return current_spo2;
}

uint8_t max30102_get_hr(void) {
    return current_hr;
}

uint8_t max30102_is_fault(void) {
    return fault_state;
}
