#include "../include/workload.h"
#include "../include/emulation.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

/**
 * @brief MAP PHASE (formerly phase3_scalable_compute)
 * 
 * This function simulates an embarrassingly parallel "Map" operation.
 * Each thread performs a computationally-intensive loop on its own private data.
 * There is NO explicit data sharing between threads or sockets.
 *
 * CRITICAL EXPERIMENT RATIONALE:
 * The purpose of this phase is to measure the underlying "coherence tax" of a 
 * given architecture. In the SYSTEM_FULLY_COHERENT model, even though no data
 * is shared, the hardware's cache coherence protocol must still operate across
 * the inter-socket interconnect, generating snoop traffic. This reveals a 
 * fundamental scalability bottleneck. In contrast, the other models avoid this
 * tax by not using hardware coherence between nodes, resulting in much better
 * performance in this phase.
 */
void map_phase(thread_context_t* ctx) {
    timer_start(&ctx->phase_timers[2]);

    // Simulate independent computation on private stack data.
    volatile int result = 0;
    for (int i = 0; i < ctx->config->compute_cycles; i++) {
        result += i; // This is just to keep the CPU busy.
    }

    timer_stop(&ctx->phase_timers[2]);
}

/**
 * @brief SHUFFLE PHASE (formerly phase2_inter_node_barrier)
 * 
 * This function simulates the coordination/synchronization step that would
 * occur between the Map and Reduce phases. It models a lightweight barrier
 * where each node must signal completion of its Map tasks.
 */
void shuffle_phase(thread_context_t* ctx) {
    timer_start(&ctx->phase_timers[1]);

    // A simple barrier to ensure all threads are done with the map phase.
    // First, a local barrier within the socket.
    int socket_base_thread_id = ctx->socket_id * ctx->config->num_threads_per_socket;
    if (ctx->thread_id == socket_base_thread_id) {
        // The first thread of each socket waits for all its peers.
        for (int i = 1; i < ctx->config->num_threads_per_socket; i++) {
            while (ctx->shared[ctx->socket_id].barrier_count < i) { usleep(1); }
        }
    } else {
        __sync_fetch_and_add(&ctx->shared[ctx->socket_id].barrier_count, 1);
    }

    // Then, a global barrier across sockets.
    // Only the first thread of each socket participates in the global barrier.
    if (ctx->thread_id == socket_base_thread_id) {
        generic_lock_acquire(ctx->shared[ctx->socket_id].inter_node_lock, ctx->thread_id);
        // In a real scenario, this is where nodes would exchange data pointers.
        generic_lock_release(ctx->shared[ctx->socket_id].inter_node_lock, ctx->thread_id);
    }

    timer_stop(&ctx->phase_timers[1]);
}

/**
 * @brief REDUCE PHASE (formerly phase1_intra_node_reduce)
 * 
 * This function simulates a "Reduce" operation where all threads within a single
 * node (socket) cooperatively update a shared data structure. This phase is
 * designed to stress the intra-node coherence mechanism.
 */
void reduce_phase(thread_context_t* ctx) {
    timer_start(&ctx->phase_timers[0]);

    // Each thread repeatedly acquires a lock and increments a shared counter.
    for (int i = 0; i < ctx->config->increments_per_thread; i++) {
        generic_lock_acquire(ctx->shared[ctx->socket_id].intra_node_lock, ctx->thread_id);
        ctx->shared[ctx->socket_id].counter++;
        generic_lock_release(ctx->shared[ctx->socket_id].intra_node_lock, ctx->thread_id);
    }

    timer_stop(&ctx->phase_timers[0]);
}


// Main thread function that executes the workload phases in order.
void* workload_thread(void* arg) {
    thread_context_t* ctx = (thread_context_t*)arg;

    // Pin this thread to its assigned core for stable measurements.
    pin_thread_to_core(ctx->core_id);

    // Wait for the master thread to signal the start.
    pthread_barrier_wait(ctx->start_barrier);

    timer_start(&ctx->total_timer);

    // Execute the workload phases in the MapReduce order.
    map_phase(ctx);
    shuffle_phase(ctx);
    reduce_phase(ctx);

    timer_stop(&ctx->total_timer);

    return NULL;
}

// Initialize shared data structures
void init_shared_data(shared_data_t* shared_data_array, workload_config_t* config, 
                     generic_lock_t* intra_locks, generic_lock_t* inter_locks) {
    for (int i = 0; i < config->total_sockets; i++) {
        shared_data_array[i].counter = 0;
        shared_data_array[i].barrier_count = 0;
        shared_data_array[i].intra_node_lock = &intra_locks[i];
        shared_data_array[i].inter_node_lock = &inter_locks[i];
    }
}

// Cleanup shared data
void cleanup_shared_data(shared_data_t* shared) {
    // Nothing to do for this workload, but good practice to have.
}
