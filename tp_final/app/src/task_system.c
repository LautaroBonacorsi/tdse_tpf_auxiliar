#include "task_system.h"
#include "main.h"
#include "ui.h"
#include "telemetry.h"
#include "lcd_i2c.h"
#include "eeprom_at24c.h"
#include "max30102.h"

static system_state_t current_state = STATE_INIT;
static uint32_t last_tick = 0;

void task_system_init(void) {
    ui_init();
    lcd_i2c_init();
    eeprom_init();
    telemetry_init();
    
    // Check if MAX30102 is present
    if (max30102_init() != 0) {
        current_state = STATE_FAULT;
        lcd_i2c_print("ERR: SENSOR OFF");
        ui_set_fault(1);
    } else {
        current_state = STATE_NORMAL;
        lcd_i2c_print("MMSV NORMAL");
    }
}

void task_system_update(void) {
    uint32_t current_tick = HAL_GetTick();
    if ((current_tick - last_tick) >= 1) {
        last_tick = current_tick;
        
        ui_update();
        telemetry_update();
        max30102_update();
        
        switch (current_state) {
            case STATE_INIT:
                break;
                
            case STATE_NORMAL:
                if (max30102_is_fault()) {
                    current_state = STATE_FAULT;
                    ui_set_fault(1);
                    lcd_i2c_print("ERR: SENSOR OFF");
                    telemetry_send("FALLA_SENSOR\r\n");
                } else if (max30102_get_spo2() > 0 && max30102_get_spo2() < eeprom_get_spo2_threshold()) {
                    current_state = STATE_MEDICAL_ALARM;
                    ui_set_alarm(1);
                    lcd_i2c_print("LOW SPO2!");
                    telemetry_send("ALERTA_CRITICA\r\n");
                }
                break;
                
            case STATE_SETUP:
                break;
                
            case STATE_MEDICAL_ALARM:
                if (max30102_get_spo2() >= eeprom_get_spo2_threshold()) {
                    current_state = STATE_NORMAL;
                    ui_set_alarm(0);
                    lcd_i2c_print("MMSV NORMAL");
                }
                // Allow UI button to silence alarm (Acknowledge)
                if (ui_get_ack_flag()) {
                    ui_clear_ack_flag();
                    ui_set_alarm(0); // Silence the sound, but maybe keep LED or state
                    current_state = STATE_NORMAL; 
                    lcd_i2c_print("MMSV SILENCED");
                }
                break;
                
            case STATE_FAULT:
                // Needs physical reset or a reconnection
                if (!max30102_is_fault()) {
                    current_state = STATE_NORMAL;
                    ui_set_fault(0);
                    lcd_i2c_print("MMSV NORMAL");
                }
                break;
        }
    }
}

system_state_t task_system_get_state(void) {
    return current_state;
}

void task_system_set_state(system_state_t new_state) {
    current_state = new_state;
}
