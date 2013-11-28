#include <stdlib.h>
#include <time.h>
#include <stdint.h>

#include "periodic.h"

extern int clock_nanosleep(clockid_t __clock_id, int __flags,
			   __const struct timespec *__req,
			   struct timespec *__rem);

struct periodic_task {
    struct timespec r;
    int period;
};
#define NSEC_PER_SEC 1000000000ULL

static inline void timespec_add_us(struct timespec *t, uint64_t d)
{
    d *= 1000;
    d += t->tv_nsec;
    while (d >= NSEC_PER_SEC) {
      d -= NSEC_PER_SEC;
      t->tv_sec += 1;
    }
    t->tv_nsec = d;
}

/* Check the timer and set the next activation */
void wait_next_activation(struct periodic_task *pd)
{
    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &pd->r, NULL);
    timespec_add_us(&pd->r, pd->period);
}

/* Clock setting */
struct periodic_task *start_periodic_timer(uint64_t offs, int t)
{
    struct periodic_task *task;

    task = (periodic_task*) malloc(sizeof(struct periodic_task));
    clock_gettime(CLOCK_REALTIME, &task->r);
    timespec_add_us(&task->r, offs);
    task->period = t;

    return task;
}
