#ifndef WORKLOAD_H
#define WORKLOAD_H

#include "sync.h"
#include "timer.h"

// Workload configuration
typedef struct {
    int num_threads_per_socket;
    int increments_per_thread;
    int compute_cycles;
    int total_sockets;
} workload_config_t;

// Shared data structures
typedef struct {
    volatile int counter;
    volatile int barrier_count;
    volatile bool barrier_sense;
    generic_lock_t* intra_node_lock;
    generic_lock_t* inter_node_lock;
} shared_data_t;

// Thread context
typedef struct {
    int thread_id;
    int socket_id;
    int core_id;
    workload_config_t* config;
    shared_data_t* shared;
    timer phase_timers[3];
    timer total_timer;
} thread_context_t;

// Phase functions
void phase1_intra_node_reduce(thread_context_t* ctx);
void phase2_inter_node_barrier(thread_context_t* ctx);
void phase3_scalable_compute(thread_context_t* ctx);

// Workload execution
void* workload_thread(void* arg);
void init_shared_data(shared_data_t* shared, workload_config_t* config, 
                     generic_lock_t* intra_lock, generic_lock_t* inter_lock);
void cleanup_shared_data(shared_data_t* shared);

#endif // WORKLOAD_H