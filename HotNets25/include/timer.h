#ifndef TIMER_H
#define TIMER_H

#include <time.h>

typedef struct {
    struct timespec start;
    struct timespec stop;
} timer;

void timer_start(timer* t);
void timer_stop(timer* t);
double timer_get_elapsed_ns(timer* t);

#endif // TIMER_H
