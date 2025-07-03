#include "../include/workload.h"
#include "../include/emulation.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

// Phase 1: Intra-Node Parallel Work (Local Reduce)
void phase1_intra_node_reduce(thread_context_t* ctx) {
    timer_start(&ctx->phase_timers[0]);
    
    // Each thread increments the shared counter multiple times
    for (int i = 0; i < ctx->config->increments_per_thread; i++) {
        generic_lock_acquire(ctx->shared->intra_node_lock, ctx->thread_id);
        ctx->shared->counter++;
        generic_lock_release(ctx->shared->intra_node_lock, ctx->thread_id);
    }
    
    timer_stop(&ctx->phase_timers[0]);
}

// Phase 2: Inter-Node Coordination (Global Barrier)
void phase2_inter_node_barrier(thread_context_t* ctx) {
    timer_start(&ctx->phase_timers[1]);
    
    // Only one thread per socket participates in inter-node coordination
    if (ctx->thread_id % ctx->config->num_threads_per_socket == 0) {
        generic_lock_acquire(ctx->shared->inter_node_lock, ctx->socket_id);
        
        // Simulate global coordination work
        ctx->shared->barrier_count++;
        
        // Flush cache line for inter-node visibility
        flush_cache_line((void*)&ctx->shared->barrier_count);
        
        generic_lock_release(ctx->shared->inter_node_lock, ctx->socket_id);
    }
    
    // Local barrier - wait for all threads in socket to reach this point
    static volatile int local_barrier_counter = 0;
    static volatile bool local_barrier_sense = false;
    
    __sync_fetch_and_add(&local_barrier_counter, 1);
    bool my_sense = local_barrier_sense;
    
    if (local_barrier_counter == ctx->config->num_threads_per_socket) {
        local_barrier_counter = 0;
        local_barrier_sense = !local_barrier_sense;
    } else {
        while (local_barrier_sense == my_sense) {
            // Spin wait
        }
    }
    
    timer_stop(&ctx->phase_timers[1]);
}

// Phase 3: Scalable Parallel Compute
void phase3_scalable_compute(thread_context_t* ctx) {
    timer_start(&ctx->phase_timers[2]);
    
    // Simulate independent computation
    volatile int result = 0;
    for (int i = 0; i < ctx->config->compute_cycles; i++) {
        result += i * ctx->thread_id;
        
        // Inject coherence tax for fully coherent system
        // This is determined by the lock type used
        if (strstr(ctx->shared->intra_node_lock->name, "Hardware") != NULL &&
            strstr(ctx->shared->inter_node_lock->name, "Hardware") != NULL) {
            // This is the fully coherent system - inject tax
            inject_coherence_tax(0); // Use default calibrated cycles
        }
    }
    
    timer_stop(&ctx->phase_timers[2]);
}

// Main thread function
void* workload_thread(void* arg) {
    thread_context_t* ctx = (thread_context_t*)arg;
    
    // Pin thread to specific core
    pin_thread_to_core(ctx->core_id);
    
    // Start total timer
    timer_start(&ctx->total_timer);
    
    // Execute the three phases
    phase1_intra_node_reduce(ctx);
    phase2_inter_node_barrier(ctx);
    phase3_scalable_compute(ctx);
    
    // Stop total timer
    timer_stop(&ctx->total_timer);
    
    return NULL;
}

// Initialize shared data structures
void init_shared_data(shared_data_t* shared, workload_config_t* config, 
                     generic_lock_t* intra_lock, generic_lock_t* inter_lock) {
    shared->counter = 0;
    shared->barrier_count = 0;
    shared->barrier_sense = false;
    shared->intra_node_lock = intra_lock;
    shared->inter_node_lock = inter_lock;
}

// Cleanup shared data
void cleanup_shared_data(shared_data_t* shared) {
    // Nothing to cleanup for now
    (void)shared;
}
