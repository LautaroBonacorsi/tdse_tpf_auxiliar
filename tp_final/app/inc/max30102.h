#ifndef MAX30102_H
#define MAX30102_H

#include <stdint.h>

uint8_t max30102_init(void);
void max30102_update(void);
void max30102_exti_callback(void);

uint8_t max30102_get_spo2(void);
uint8_t max30102_get_hr(void);
uint8_t max30102_is_fault(void);

#endif /* MAX30102_H */
