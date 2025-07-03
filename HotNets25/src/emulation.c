#define _GNU_SOURCE
#include "../include/emulation.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/sysinfo.h>

// Global topology information
static int total_cores = 0;
static int total_sockets = 0;
static int cores_per_socket = 0;
static int* core_to_socket_map = NULL;
static bool topology_detected = false;

// Coherence tax calibration
static volatile int coherence_tax_cycles = 0;

void pin_thread_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_t current_thread = pthread_self();
    if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset) != 0) {
        perror("pthread_setaffinity_np");
        exit(EXIT_FAILURE);
    }
}

int get_socket_for_core(int core_id) {
    if (!topology_detected) {
        detect_numa_topology();
    }
    
    if (core_id < 0 || core_id >= total_cores) {
        return -1;
    }
    
    return core_to_socket_map[core_id];
}

int get_cores_per_socket(void) {
    if (!topology_detected) {
        detect_numa_topology();
    }
    return cores_per_socket;
}

int get_total_sockets(void) {
    if (!topology_detected) {
        detect_numa_topology();
    }
    return total_sockets;
}

void detect_numa_topology(void) {
    if (topology_detected) return;
    
    total_cores = get_nprocs();
    core_to_socket_map = malloc(total_cores * sizeof(int));
    
    // Try to read socket information from /sys
    int max_socket = -1;
    for (int core = 0; core < total_cores; core++) {
        char path[256];
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/topology/physical_package_id", core);
        
        FILE* fp = fopen(path, "r");
        if (fp) {
            int socket;
            if (fscanf(fp, "%d", &socket) == 1) {
                core_to_socket_map[core] = socket;
                if (socket > max_socket) {
                    max_socket = socket;
                }
            } else {
                core_to_socket_map[core] = 0; // Fallback
            }
            fclose(fp);
        } else {
            // Fallback: assume single socket
            core_to_socket_map[core] = 0;
        }
    }
    
    total_sockets = max_socket + 1;
    if (total_sockets <= 0) {
        total_sockets = 1; // Fallback
    }
    
    cores_per_socket = total_cores / total_sockets;
    topology_detected = true;
    
    printf("Detected topology: %d cores, %d sockets, %d cores per socket\n",
           total_cores, total_sockets, cores_per_socket);
}

void print_numa_topology(void) {
    if (!topology_detected) {
        detect_numa_topology();
    }
    
    printf("\nNUMA Topology:\n");
    printf("Total cores: %d\n", total_cores);
    printf("Total sockets: %d\n", total_sockets);
    printf("Cores per socket: %d\n", cores_per_socket);
    
    printf("\nCore to socket mapping:\n");
    for (int socket = 0; socket < total_sockets; socket++) {
        printf("Socket %d: cores ", socket);
        bool first = true;
        for (int core = 0; core < total_cores; core++) {
            if (core_to_socket_map[core] == socket) {
                if (!first) printf(", ");
                printf("%d", core);
                first = false;
            }
        }
        printf("\n");
    }
}

void calibrate_coherence_tax(void) {
    // Calibrate based on typical CXL vs L3 cache latency difference
    // CXL directory lookup: ~200-300ns
    // L3 cache hit: ~30-50ns
    // Difference: ~200ns
    
    // Measure CPU frequency to convert time to cycles
    struct timespec start, end;
    volatile int dummy = 0;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < 1000000; i++) {
        dummy++;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed_ns = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
    double cycles_per_ns = 1000000.0 / elapsed_ns; // Rough estimate
    
    // Target: ~200ns coherence tax
    coherence_tax_cycles = (int)(200.0 * cycles_per_ns);
    
    printf("Calibrated coherence tax: %d cycles (~200ns)\n", coherence_tax_cycles);
}

void inject_coherence_tax(int cycles) {
    if (cycles <= 0) {
        cycles = coherence_tax_cycles;
    }
    
    // Spin-wait to burn cycles
    volatile int dummy = 0;
    for (int i = 0; i < cycles; i++) {
        dummy++;
    }
}
