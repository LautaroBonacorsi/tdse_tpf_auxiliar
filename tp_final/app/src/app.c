#include "app.h"
#include "task_system.h"
#include "main.h"

void app_init(void) {
    task_system_init();
}

void app_update(void) {
    task_system_update();
}
