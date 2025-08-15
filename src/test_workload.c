#include "workload.h"
#include "sync.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_THREADS 4
#define INCREMENTS 1000
#define COMPUTE_CYCLES 100000

// Dummy lock functions for testing
void dummy_lock_acquire(void* lock) {
    // Placeholder for lock acquire logic
}

void dummy_lock_release(void* lock) {
    // Placeholder for lock release logic
}

void* thread_function(void* arg) {
    int thread_id = *(int*)arg;
    phase1_intra_node_reduce(dummy_lock_acquire, dummy_lock_release, NULL, INCREMENTS, thread_id);
    phase2_inter_node_barrier(dummy_lock_acquire, dummy_lock_release, NULL, thread_id);
    phase3_scalable_compute(COMPUTE_CYCLES);
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, thread_function, &thread_ids[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Workload test completed successfully.\n");
    return 0;
}
