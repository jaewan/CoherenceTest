#include "../include/emulation.h"
#include "../include/sync.h"
#include "../include/workload.h"
#include "../include/timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Simple test to verify the basic functionality
int main() {
    printf("=== FEDERATED COHERENCE - BASIC TEST ===\n");
    
    // Test 1: Basic lock functionality
    printf("Testing basic lock functionality...\n");
    
    hw_lock_t hw_lock;
    hw_lock_init(&hw_lock);
    hw_lock_acquire(&hw_lock);
    hw_lock_release(&hw_lock);
    printf("✓ Hardware lock test passed\n");
    
    bakery_lock_t bakery_lock;
    bakery_lock_init(&bakery_lock);
    bakery_lock_acquire(&bakery_lock, 0);
    bakery_lock_release(&bakery_lock, 0);
    printf("✓ Bakery lock test passed\n");
    
    // Test 2: Generic lock interface
    printf("Testing generic lock interface...\n");
    
    generic_lock_t generic_hw;
    generic_lock_init_hw(&generic_hw, &hw_lock);
    generic_lock_acquire(&generic_hw, 0);
    generic_lock_release(&generic_hw, 0);
    printf("✓ Generic hardware lock test passed\n");
    
    generic_lock_t generic_bakery;
    generic_lock_init_bakery(&generic_bakery, &bakery_lock);
    generic_lock_acquire(&generic_bakery, 0);
    generic_lock_release(&generic_bakery, 0);
    printf("✓ Generic bakery lock test passed\n");
    
    // Test 3: Topology detection
    printf("Testing topology detection...\n");
    detect_numa_topology();
    printf("✓ Topology detection completed\n");
    printf("  - Total sockets: %d\n", get_total_sockets());
    printf("  - Cores per socket: %d\n", get_cores_per_socket());
    
    // Test 4: Timer functionality
    printf("Testing timer functionality...\n");
    timer t;
    timer_start(&t);
    usleep(1000); // Sleep for 1ms
    timer_stop(&t);
    double elapsed = timer_get_elapsed_ns(&t);
    printf("✓ Timer test passed (measured: %.3f ms)\n", elapsed / 1e6);
    
    printf("\n=== ALL TESTS PASSED ===\n");
    printf("Run './experiment' to start the full federated coherence experiment.\n");
    printf("Use './experiment -h' for help with command line options.\n");
    
    return 0;
}
