#ifndef SYNC_H
#define SYNC_H

#include <pthread.h>
#include <stdbool.h>
#include <xmmintrin.h>

#define MAX_THREADS 64

// Hardware lock types
typedef volatile int hw_lock_t;

// Bakery lock structure
typedef struct {
    volatile bool choosing[MAX_THREADS];
    volatile int ticket[MAX_THREADS];
} bakery_lock_t;

// Generic lock interface for system switching
typedef struct {
    volatile void* lock_data;
    void (*acquire)(volatile void* lock, int thread_id);
    void (*release)(volatile void* lock, int thread_id);
    const char* name;
} generic_lock_t;

// Hardware lock functions
void hw_lock_init(hw_lock_t* lock);
void hw_lock_acquire(hw_lock_t* lock);
void hw_lock_release(hw_lock_t* lock);

// Bakery lock functions
void bakery_lock_init(bakery_lock_t* lock);
void bakery_lock_acquire(bakery_lock_t* lock, int thread_id);
void bakery_lock_release(bakery_lock_t* lock, int thread_id);

// Generic lock interface functions
void generic_lock_init_hw(generic_lock_t* lock, hw_lock_t* hw_lock);
void generic_lock_init_bakery(generic_lock_t* lock, bakery_lock_t* bakery_lock);
void generic_lock_acquire(generic_lock_t* lock, int thread_id);
void generic_lock_release(generic_lock_t* lock, int thread_id);

// Cache management
static inline void flush_cache_line(void* addr) {
    _mm_clflush(addr);
}

static inline void memory_barrier(void) {
    __sync_synchronize();
}

#endif // SYNC_H