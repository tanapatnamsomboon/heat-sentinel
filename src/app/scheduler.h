#ifndef APP_SCHEDULER_H
#define APP_SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>

/* Maximum number of registered tasks (override at build time if needed). */
#ifndef SCHED_MAX_TASKS
#define SCHED_MAX_TASKS 8
#endif

typedef void (*sched_fn_t)(void);

/*
 * Cooperative, non-preemptive scheduler driven by millis().
 *
 * Register periodic jobs with sched_add(), then call sched_tick() repeatedly
 * from the main loop. Each job runs to completion before the next is considered,
 * so jobs must be short and non-blocking: a job that overruns another job's
 * period merely delays it, it never preempts.
 *
 * period_ms == 0 means "run on every sched_tick()".
 */

/* Register a task. Returns false if fn is NULL or the table is full. */
bool sched_add(uint16_t period_ms, sched_fn_t fn);

/* Run every task that is currently due. Call this from the superloop. */
void sched_tick(void);

#endif /* APP_SCHEDULER_H */
