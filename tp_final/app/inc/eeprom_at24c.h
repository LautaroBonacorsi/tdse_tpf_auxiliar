#ifndef EEPROM_AT24C_H
#define EEPROM_AT24C_H

#include <stdint.h>

void eeprom_init(void);
uint8_t eeprom_get_spo2_threshold(void);
void eeprom_set_spo2_threshold(uint8_t value);

#endif /* EEPROM_AT24C_H */
