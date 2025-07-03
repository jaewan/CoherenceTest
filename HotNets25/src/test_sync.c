#include "../include/sync.h"

int main() {
    hw_lock_t lock;
    hw_lock_init(&lock);
    hw_lock_acquire(&lock);
    hw_lock_release(&lock);

    bakery_lock_t bakery_lock;
    bakery_lock_init(&bakery_lock);
    bakery_lock_acquire(&bakery_lock, 0);
    bakery_lock_release(&bakery_lock, 0);

    return 0;
}
