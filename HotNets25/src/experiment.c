#include "../include/emulation.h"
#include "../include/sync.h"
#include "../include/workload.h"
#include "../include/timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

// Experiment configuration
typedef struct {
    system_type_t system_type;
    int num_threads_per_socket;
    int increments_per_thread;
    int compute_cycles;
    bool verbose;
} experiment_config_t;

// Results structure
typedef struct {
    double phase1_avg_ns;
    double phase2_avg_ns;
    double phase3_avg_ns;
    double total_avg_ns;
    const char* system_name;
} experiment_results_t;

// System configuration functions
static void setup_system_locks(system_type_t system_type, int socket_id, 
                              generic_lock_t* intra_lock, generic_lock_t* inter_lock,
                              void** intra_data, void** inter_data) {
    switch (system_type) {
        case SYSTEM_FULLY_COHERENT:
            // Both intra and inter use hardware locks
            *intra_data = malloc(sizeof(hw_lock_t));
            *inter_data = malloc(sizeof(hw_lock_t));
            generic_lock_init_hw(intra_lock, (hw_lock_t*)*intra_data);
            generic_lock_init_hw(inter_lock, (hw_lock_t*)*inter_data);
            break;
            
        case SYSTEM_FEDERATED_COHERENCE:
            // Intra-node uses hardware, inter-node uses software
            *intra_data = malloc(sizeof(hw_lock_t));
            *inter_data = malloc(sizeof(bakery_lock_t));
            generic_lock_init_hw(intra_lock, (hw_lock_t*)*intra_data);
            generic_lock_init_bakery(inter_lock, (bakery_lock_t*)*inter_data);
            break;
            
        case SYSTEM_FULLY_NON_COHERENT:
            // Both use software locks
            *intra_data = malloc(sizeof(bakery_lock_t));
            *inter_data = malloc(sizeof(bakery_lock_t));
            generic_lock_init_bakery(intra_lock, (bakery_lock_t*)*intra_data);
            generic_lock_init_bakery(inter_lock, (bakery_lock_t*)*inter_data);
            break;
    }
}

static const char* get_system_name(system_type_t system_type) {
    switch (system_type) {
        case SYSTEM_FULLY_COHERENT: return "Fully Coherent";
        case SYSTEM_FEDERATED_COHERENCE: return "Federated Coherence";
        case SYSTEM_FULLY_NON_COHERENT: return "Fully Non-Coherent";
        default: return "Unknown";
    }
}

// Run experiment for a specific system type
static experiment_results_t run_experiment(experiment_config_t* config) {
    experiment_results_t results = {0};
    results.system_name = get_system_name(config->system_type);
    
    // Detect topology
    detect_numa_topology();
    if (config->verbose) {
        print_numa_topology();
    }
    
    int total_sockets = get_total_sockets();
    int cores_per_socket = get_cores_per_socket();
    int total_threads = total_sockets * config->num_threads_per_socket;
    
    if (config->verbose) {
        printf("\nRunning experiment: %s\n", results.system_name);
        printf("Threads per socket: %d\n", config->num_threads_per_socket);
        printf("Total threads: %d\n", total_threads);
        printf("Increments per thread: %d\n", config->increments_per_thread);
        printf("Compute cycles: %d\n", config->compute_cycles);
    }
    
    // Calibrate coherence tax for fully coherent system
    if (config->system_type == SYSTEM_FULLY_COHERENT) {
        calibrate_coherence_tax();
    }
    
    // Setup workload configuration
    workload_config_t workload_config = {
        .num_threads_per_socket = config->num_threads_per_socket,
        .increments_per_thread = config->increments_per_thread,
        .compute_cycles = config->compute_cycles,
        .total_sockets = total_sockets
    };
    
    // Setup locks for each socket
    generic_lock_t* intra_locks = malloc(total_sockets * sizeof(generic_lock_t));
    generic_lock_t* inter_locks = malloc(total_sockets * sizeof(generic_lock_t));
    void** intra_data = malloc(total_sockets * sizeof(void*));
    void** inter_data = malloc(total_sockets * sizeof(void*));
    
    for (int socket = 0; socket < total_sockets; socket++) {
        setup_system_locks(config->system_type, socket, 
                          &intra_locks[socket], &inter_locks[socket],
                          &intra_data[socket], &inter_data[socket]);
    }
    
    // Setup shared data
    shared_data_t shared;
    init_shared_data(&shared, &workload_config, &intra_locks[0], &inter_locks[0]);
    
    // Setup thread contexts
    thread_context_t* contexts = malloc(total_threads * sizeof(thread_context_t));
    pthread_t* threads = malloc(total_threads * sizeof(pthread_t));
    
    int thread_id = 0;
    for (int socket = 0; socket < total_sockets; socket++) {
        for (int local_thread = 0; local_thread < config->num_threads_per_socket; local_thread++) {
            contexts[thread_id].thread_id = thread_id;
            contexts[thread_id].socket_id = socket;
            contexts[thread_id].core_id = socket * cores_per_socket + local_thread;
            contexts[thread_id].config = &workload_config;
            contexts[thread_id].shared = &shared;
            
            // Use socket-specific locks
            contexts[thread_id].shared->intra_node_lock = &intra_locks[socket];
            contexts[thread_id].shared->inter_node_lock = &inter_locks[socket];
            
            thread_id++;
        }
    }
    
    // Launch threads
    for (int i = 0; i < total_threads; i++) {
        pthread_create(&threads[i], NULL, workload_thread, &contexts[i]);
    }
    
    // Wait for completion
    for (int i = 0; i < total_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Calculate average timings
    double phase1_total = 0, phase2_total = 0, phase3_total = 0, total_total = 0;
    for (int i = 0; i < total_threads; i++) {
        phase1_total += timer_get_elapsed_ns(&contexts[i].phase_timers[0]);
        phase2_total += timer_get_elapsed_ns(&contexts[i].phase_timers[1]);
        phase3_total += timer_get_elapsed_ns(&contexts[i].phase_timers[2]);
        total_total += timer_get_elapsed_ns(&contexts[i].total_timer);
    }
    
    results.phase1_avg_ns = phase1_total / total_threads;
    results.phase2_avg_ns = phase2_total / total_threads;
    results.phase3_avg_ns = phase3_total / total_threads;
    results.total_avg_ns = total_total / total_threads;
    
    // Cleanup
    for (int socket = 0; socket < total_sockets; socket++) {
        free(intra_data[socket]);
        free(inter_data[socket]);
    }
    free(intra_locks);
    free(inter_locks);
    free(intra_data);
    free(inter_data);
    free(contexts);
    free(threads);
    
    return results;
}

// Print results
static void print_results(experiment_results_t* results, int num_systems) {
    printf("\n=== EXPERIMENT RESULTS ===\n");
    printf("%-20s | %12s | %12s | %12s | %12s\n", 
           "System", "Phase 1 (ms)", "Phase 2 (ms)", "Phase 3 (ms)", "Total (ms)");
    printf("%-20s-+-%12s-+-%12s-+-%12s-+-%12s\n", 
           "--------------------", "------------", "------------", "------------", "------------");
    
    for (int i = 0; i < num_systems; i++) {
        printf("%-20s | %12.3f | %12.3f | %12.3f | %12.3f\n",
               results[i].system_name,
               results[i].phase1_avg_ns / 1e6,
               results[i].phase2_avg_ns / 1e6,
               results[i].phase3_avg_ns / 1e6,
               results[i].total_avg_ns / 1e6);
    }
    
    printf("\nPhase 1: Intra-Node Parallel Work (Local Reduce)\n");
    printf("Phase 2: Inter-Node Coordination (Global Barrier)\n");
    printf("Phase 3: Scalable Parallel Compute\n");
}

// Usage function
static void print_usage(const char* prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -s <system>    System type: coherent, federated, non-coherent, all (default: all)\n");
    printf("  -t <threads>   Threads per socket (default: 4)\n");
    printf("  -i <increments> Increments per thread (default: 1000)\n");
    printf("  -c <cycles>    Compute cycles (default: 100000)\n");
    printf("  -v             Verbose output\n");
    printf("  -h             Show this help\n");
}

int main(int argc, char* argv[]) {
    experiment_config_t config = {
        .system_type = SYSTEM_FULLY_COHERENT, // Will be overridden for "all"
        .num_threads_per_socket = 4,
        .increments_per_thread = 1000,
        .compute_cycles = 100000,
        .verbose = false
    };
    
    bool run_all = true;
    
    // Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "s:t:i:c:vh")) != -1) {
        switch (opt) {
            case 's':
                run_all = false;
                if (strcmp(optarg, "coherent") == 0) {
                    config.system_type = SYSTEM_FULLY_COHERENT;
                } else if (strcmp(optarg, "federated") == 0) {
                    config.system_type = SYSTEM_FEDERATED_COHERENCE;
                } else if (strcmp(optarg, "non-coherent") == 0) {
                    config.system_type = SYSTEM_FULLY_NON_COHERENT;
                } else if (strcmp(optarg, "all") == 0) {
                    run_all = true;
                } else {
                    fprintf(stderr, "Invalid system type: %s\n", optarg);
                    print_usage(argv[0]);
                    return 1;
                }
                break;
            case 't':
                config.num_threads_per_socket = atoi(optarg);
                break;
            case 'i':
                config.increments_per_thread = atoi(optarg);
                break;
            case 'c':
                config.compute_cycles = atoi(optarg);
                break;
            case 'v':
                config.verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    printf("=== FEDERATED COHERENCE EXPERIMENT ===\n");
    
    if (run_all) {
        experiment_results_t results[3];
        system_type_t systems[] = {SYSTEM_FULLY_COHERENT, SYSTEM_FEDERATED_COHERENCE, SYSTEM_FULLY_NON_COHERENT};
        
        for (int i = 0; i < 3; i++) {
            config.system_type = systems[i];
            results[i] = run_experiment(&config);
        }
        
        print_results(results, 3);
    } else {
        experiment_results_t result = run_experiment(&config);
        print_results(&result, 1);
    }
    
    return 0;
}
