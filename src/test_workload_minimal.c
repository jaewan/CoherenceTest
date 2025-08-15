#include "test_workload_minimal.h"
#include <stdio.h>

void phase1_intra_node_reduce(void (*lock_acquire)(void*), void (*lock_release)(void*), void* lock, int increments, int thread_id) {
    printf("Phase 1 executed.\n");
}

void phase2_inter_node_barrier(void (*lock_acquire)(void*), void (*lock_release)(void*), void* lock, int thread_id) {
    printf("Phase 2 executed.\n");
}

void phase3_scalable_compute(int compute_cycles) {
    printf("Phase 3 executed.\n");
}

int main() {
    phase1_intra_node_reduce(NULL, NULL, NULL, 0, 0);
    phase2_inter_node_barrier(NULL, NULL, NULL, 0);
    phase3_scalable_compute(0);
    return 0;
}
