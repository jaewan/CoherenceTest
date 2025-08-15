typedef volatile int hw_lock_t;

typedef struct {
    volatile int choosing[64];
    volatile int ticket[64];
} bakery_lock_t;

void hw_lock_init(hw_lock_t* lock);
void hw_lock_acquire(hw_lock_t* lock);
void hw_lock_release(hw_lock_t* lock);

void bakery_lock_init(bakery_lock_t* lock);
void bakery_lock_acquire(bakery_lock_t* lock, int thread_id);
void bakery_lock_release(bakery_lock_t* lock, int thread_id);
