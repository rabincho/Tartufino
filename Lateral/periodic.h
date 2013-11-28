#ifndef PERIODIC_H
#define PERIODIC_H
#include <stdint.h>

struct periodic_task;

void wait_next_activation(struct periodic_task *t);
struct periodic_task *start_periodic_timer(uint64_t offs, int t);

#endif
