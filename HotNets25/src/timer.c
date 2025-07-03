#include "timer.h"

void timer_start(timer* t) {
    clock_gettime(CLOCK_MONOTONIC, &t->start);
}

void timer_stop(timer* t) {
    clock_gettime(CLOCK_MONOTONIC, &t->stop);
}

double timer_get_elapsed_ns(timer* t) {
    return (t->stop.tv_sec - t->start.tv_sec) * 1e9 +
           (t->stop.tv_nsec - t->start.tv_nsec);
}
