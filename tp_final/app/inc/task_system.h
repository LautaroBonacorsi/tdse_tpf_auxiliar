#ifndef TASK_SYSTEM_H
#define TASK_SYSTEM_H

#include <stdint.h>

typedef enum {
    STATE_INIT,
    STATE_NORMAL,
    STATE_SETUP,
    STATE_MEDICAL_ALARM,
    STATE_FAULT
} system_state_t;

void task_system_init(void);
void task_system_update(void);

system_state_t task_system_get_state(void);
void task_system_set_state(system_state_t new_state);

#endif /* TASK_SYSTEM_H */
