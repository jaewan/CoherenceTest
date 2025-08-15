void phase1_intra_node_reduce(void (*lock_acquire)(void*), void (*lock_release)(void*), void* lock, int increments, int thread_id);
void phase2_inter_node_barrier(void (*lock_acquire)(void*), void (*lock_release)(void*), void* lock, int thread_id);
void phase3_scalable_compute(int compute_cycles);
