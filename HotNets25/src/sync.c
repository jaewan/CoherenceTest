#include "../include/sync.h"
#include <stdio.h>

// Hardware-assisted spinlock implementation
void hw_lock_init(hw_lock_t* lock) {
    *lock = 0;
}

void hw_lock_acquire(hw_lock_t* lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        while (*lock); // Spin-wait
    }
}

void hw_lock_release(hw_lock_t* lock) {
    __sync_lock_release(lock);
}

// Lamport's Bakery Lock implementation
void bakery_lock_init(bakery_lock_t* lock) {
    for (int i = 0; i < MAX_THREADS; i++) {
        lock->choosing[i] = false;
        lock->ticket[i] = 0;
    }
}

static int max_ticket(bakery_lock_t* lock) {
    int max = 0;
    for (int i = 0; i < MAX_THREADS; i++) {
        if (lock->ticket[i] > max) {
            max = lock->ticket[i];
        }
    }
    return max;
}

void bakery_lock_acquire(bakery_lock_t* lock, int thread_id) {
    lock->choosing[thread_id] = true;
    memory_barrier();
    lock->ticket[thread_id] = max_ticket(lock) + 1;
    memory_barrier();
    lock->choosing[thread_id] = false;
    memory_barrier();

    for (int other = 0; other < MAX_THREADS; other++) {
        if (other == thread_id) continue;
        while (lock->choosing[other]); // Wait if other thread is choosing
        memory_barrier();
        while (lock->ticket[other] != 0 &&
               (lock->ticket[other] < lock->ticket[thread_id] ||
                (lock->ticket[other] == lock->ticket[thread_id] && other < thread_id)));
    }
}

void bakery_lock_release(bakery_lock_t* lock, int thread_id) {
    memory_barrier();
    lock->ticket[thread_id] = 0;
}

// Generic lock interface wrappers
static void hw_lock_acquire_wrapper(void* lock, int thread_id) {
    hw_lock_acquire((hw_lock_t*)lock);
}

static void hw_lock_release_wrapper(void* lock, int thread_id) {
    hw_lock_release((hw_lock_t*)lock);
}

static void bakery_lock_acquire_wrapper(void* lock, int thread_id) {
    bakery_lock_acquire((bakery_lock_t*)lock, thread_id);
}

static void bakery_lock_release_wrapper(void* lock, int thread_id) {
    bakery_lock_release((bakery_lock_t*)lock, thread_id);
}

void generic_lock_init_hw(generic_lock_t* lock, hw_lock_t* hw_lock) {
    hw_lock_init(hw_lock);
    lock->lock_data = hw_lock;
    lock->acquire = hw_lock_acquire_wrapper;
    lock->release = hw_lock_release_wrapper;
    lock->name = "Hardware Lock";
}

void generic_lock_init_bakery(generic_lock_t* lock, bakery_lock_t* bakery_lock) {
    bakery_lock_init(bakery_lock);
    lock->lock_data = bakery_lock;
    lock->acquire = bakery_lock_acquire_wrapper;
    lock->release = bakery_lock_release_wrapper;
    lock->name = "Bakery Lock";
}

void generic_lock_acquire(generic_lock_t* lock, int thread_id) {
    lock->acquire(lock->lock_data, thread_id);
}

void generic_lock_release(generic_lock_t* lock, int thread_id) {
    lock->release(lock->lock_data, thread_id);
}
