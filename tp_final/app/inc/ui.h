#ifndef UI_H
#define UI_H

#include <stdint.h>

void ui_init(void);
void ui_update(void);

void ui_set_alarm(uint8_t state);
void ui_set_fault(uint8_t state);

uint8_t ui_get_ack_flag(void);
void ui_clear_ack_flag(void);

void ui_exti_callback(uint16_t GPIO_Pin);

#endif /* UI_H */
