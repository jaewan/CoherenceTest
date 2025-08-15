#include "standalone_sync.h"
#include <stdio.h>

int main() {
    hw_lock_t lock;
    hw_lock_init(&lock);
    printf("Standalone test passed.\n");
    return 0;
}
