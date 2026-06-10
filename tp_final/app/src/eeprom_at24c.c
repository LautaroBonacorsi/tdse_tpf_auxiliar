#include "eeprom_at24c.h"
#include "main.h"

extern I2C_HandleTypeDef hi2c1;

// AT24C32 address on DS1307 module
#define EEPROM_ADDR (0x50 << 1)
#define SPO2_ADDR 0x0000

static uint8_t spo2_threshold = 90;

void eeprom_init(void) {
    uint8_t value = 0;
    if (HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, SPO2_ADDR, I2C_MEMADD_SIZE_16BIT, &value, 1, 100) == HAL_OK) {
        if (value > 50 && value <= 100) {
            spo2_threshold = value;
        } else {
            eeprom_set_spo2_threshold(90);
        }
    }
}

uint8_t eeprom_get_spo2_threshold(void) {
    return spo2_threshold;
}

void eeprom_set_spo2_threshold(uint8_t value) {
    spo2_threshold = value;
    HAL_I2C_Mem_Write(&hi2c1, EEPROM_ADDR, SPO2_ADDR, I2C_MEMADD_SIZE_16BIT, &value, 1, 100);
    HAL_Delay(5); // Internal write cycle time
}
