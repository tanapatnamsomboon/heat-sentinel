#include "app/scheduler.h"
#include "hal/timer.h"

#include <stddef.h>

typedef struct {
    sched_fn_t fn;
    uint32_t   last_run_ms;
    uint16_t   period_ms;
} sched_task_t;

static sched_task_t s_tasks[SCHED_MAX_TASKS];
static uint8_t      s_task_count;

bool sched_add(uint16_t period_ms, sched_fn_t fn)
{
    if (fn == NULL || s_task_count >= SCHED_MAX_TASKS) {
        return false;
    }
    s_tasks[s_task_count].fn          = fn;
    s_tasks[s_task_count].period_ms   = period_ms;
    s_tasks[s_task_count].last_run_ms = millis();
    s_task_count++;
    return true;
}

void sched_tick(void)
{
    const uint32_t now = millis();
    for (uint8_t i = 0; i < s_task_count; i++) {
        /* Unsigned subtraction => correct across the millis() wraparound. */
        if ((uint32_t)(now - s_tasks[i].last_run_ms) >= s_tasks[i].period_ms) {
            s_tasks[i].last_run_ms = now;
            s_tasks[i].fn();
        }
    }
}
