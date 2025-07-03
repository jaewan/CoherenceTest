#ifndef EMULATION_H
#define EMULATION_H

#include <sched.h>
#include <pthread.h>

// System types for emulation
typedef enum {
    SYSTEM_FULLY_COHERENT,      // System A: Full hardware coherence with tax
    SYSTEM_FEDERATED_COHERENCE, // System B: Our proposal (intra-node HW, inter-node SW)
    SYSTEM_FULLY_NON_COHERENT   // System C: All software synchronization
} system_type_t;

// Core emulation functions
void pin_thread_to_core(int core_id);
int get_socket_for_core(int core_id);
int get_cores_per_socket(void);
int get_total_sockets(void);

// Coherence tax injection
void inject_coherence_tax(int cycles);
void calibrate_coherence_tax(void);

// NUMA topology detection
void detect_numa_topology(void);
void print_numa_topology(void);

#endif // EMULATION_H