#ifndef UTILS_H
#define UTILS_H

#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

// Pins the calling thread to a specific CPU core.
void pin_thread_to_cpu(int cpu_core);

// Gets the NUMA node for a given CPU core.
int get_cpu_node(int cpu_core);

// Gets the current time in nanoseconds.
long long get_time_ns();

#endif // UTILS_H
