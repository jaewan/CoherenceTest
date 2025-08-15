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
    double map_phase_avg_ns;
    double shuffle_phase_avg_ns;
    double reduce_phase_avg_ns;
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
    int total_sockets = get_total_sockets();
    if (total_sockets < 2) {
        printf("\nWARNING: This experiment is designed for multi-socket systems, but only %d socket was detected.\n", total_sockets);
        printf("The results will not demonstrate inter-socket coherence effects.\n");
    }
    int total_threads = total_sockets * config->num_threads_per_socket;

    if (config->verbose) {
        printf("\n--- Running Experiment: %s ---\n", results.system_name);
        printf("Configuration: %d threads (%d per socket across %d sockets)\n", 
               total_threads, config->num_threads_per_socket, total_sockets);
    }

    // Allocate resources
    pthread_t* threads = malloc(total_threads * sizeof(pthread_t));
    thread_context_t* contexts = malloc(total_threads * sizeof(thread_context_t));
    shared_data_t* shared_data = malloc(total_sockets * sizeof(shared_data_t));
    generic_lock_t* intra_locks = malloc(total_sockets * sizeof(generic_lock_t));
    generic_lock_t* inter_locks = malloc(total_sockets * sizeof(generic_lock_t));
    void** intra_lock_data = malloc(total_sockets * sizeof(void*));
    void** inter_lock_data = malloc(total_sockets * sizeof(void*));
    pthread_barrier_t start_barrier;

    pthread_barrier_init(&start_barrier, NULL, total_threads + 1);

    // Create a workload_config_t from the experiment_config_t to pass to the workload module.
    workload_config_t workload_conf = {
        .num_threads_per_socket = config->num_threads_per_socket,
        .increments_per_thread = config->increments_per_thread,
        .compute_cycles = config->compute_cycles,
        .system_type = config->system_type,
        .total_sockets = total_sockets
    };

    // Setup locks for each socket based on the system type
    for (int i = 0; i < total_sockets; i++) {
        setup_system_locks(config->system_type, i, &intra_locks[i], &inter_locks[i], &intra_lock_data[i], &inter_lock_data[i]);
    }
    init_shared_data(shared_data, &workload_conf, intra_locks, inter_locks);

    // Create and configure threads
    for (int i = 0; i < total_threads; i++) {
        int socket_id = i / config->num_threads_per_socket;
        contexts[i] = (thread_context_t){
            .thread_id = i,
            .socket_id = socket_id,
            .core_id = i, // Simple mapping: thread i -> core i
            .config = &workload_conf, // Pass pointer to workload-specific config
            .shared = shared_data,
            .start_barrier = &start_barrier
        };
        pin_thread_to_core(i);
        pthread_create(&threads[i], NULL, workload_thread, &contexts[i]);
    }

    // Start all threads simultaneously
    pthread_barrier_wait(&start_barrier);

    // Wait for all threads to complete
    for (int i = 0; i < total_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // Aggregate results
    double total_map = 0, total_shuffle = 0, total_reduce = 0, total_overall = 0;
    for (int i = 0; i < total_threads; i++) {
        total_map += timer_get_elapsed_ns(&contexts[i].phase_timers[2]);
        total_shuffle += timer_get_elapsed_ns(&contexts[i].phase_timers[1]);
        total_reduce += timer_get_elapsed_ns(&contexts[i].phase_timers[0]);
        total_overall += timer_get_elapsed_ns(&contexts[i].total_timer);
    }

    results.map_phase_avg_ns = total_map / total_threads;
    results.shuffle_phase_avg_ns = total_shuffle / total_threads;
    results.reduce_phase_avg_ns = total_reduce / total_threads;
    results.total_avg_ns = total_overall / total_threads;

    // Cleanup
    for (int i = 0; i < total_sockets; i++) {
        free(intra_lock_data[i]);
        free(inter_lock_data[i]);
    }
    free(threads);
    free(contexts);
    free(shared_data);
    free(intra_locks);
    free(inter_locks);
    free(intra_lock_data);
    free(inter_lock_data);
    pthread_barrier_destroy(&start_barrier);

    return results;
}

// Print results
static void print_results(experiment_results_t* results, int num_systems) {
    printf("\n--- Experiment Results ---\n");
    printf("%-25s %15s %15s %15s %15s\n", "System", "Map (ms)", "Shuffle (ms)", "Reduce (ms)", "Total (ms)");
    printf("%-25s %15s %15s %15s %15s\n", "-------------------------", "---------------", "---------------", "---------------", "---------------");

    for (int i = 0; i < num_systems; i++) {
        printf("%-25s %15.3f %15.3f %15.3f %15.3f\n", 
               results[i].system_name, 
               results[i].map_phase_avg_ns / 1e6, 
               results[i].shuffle_phase_avg_ns / 1e6, 
               results[i].reduce_phase_avg_ns / 1e6, 
               results[i].total_avg_ns / 1e6);
    }
}

// Usage function
static void print_usage(const char* prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -s <system>     System type: coherent, federated, non-coherent, all (default: all)\n");
    printf("  -t <threads>    Threads per socket (default: 4)\n");
    printf("  -i <increments> Increments per thread (default: 1000)\n");
    printf("  -c <cycles>     Compute cycles (default: 100000)\n");
    printf("  -n <trials>     Number of trials to run and average (default: 5)\n");
    printf("  -v              Verbose output\n");
    printf("  -h              Show this help\n");
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
    int num_trials = 5; // Default number of trials
    
    // Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "s:t:i:c:n:vh")) != -1) {
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
            case 'n':
                num_trials = atoi(optarg);
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
    printf("Running %d trial(s) for each system...\n", num_trials);
    
    if (run_all) {
        experiment_results_t final_results[3];
        system_type_t systems[] = {SYSTEM_FULLY_COHERENT, SYSTEM_FEDERATED_COHERENCE, SYSTEM_FULLY_NON_COHERENT};
        
        for (int i = 0; i < 3; i++) {
            // Initialize aggregated results for this system
            final_results[i] = (experiment_results_t){ .system_name = get_system_name(systems[i]) };

            for (int trial = 0; trial < num_trials; trial++) {
                config.system_type = systems[i];
                experiment_results_t trial_result = run_experiment(&config);
                final_results[i].map_phase_avg_ns += trial_result.map_phase_avg_ns;
                final_results[i].shuffle_phase_avg_ns += trial_result.shuffle_phase_avg_ns;
                final_results[i].reduce_phase_avg_ns += trial_result.reduce_phase_avg_ns;
                final_results[i].total_avg_ns += trial_result.total_avg_ns;
            }

            // Average the results
            final_results[i].map_phase_avg_ns /= num_trials;
            final_results[i].shuffle_phase_avg_ns /= num_trials;
            final_results[i].reduce_phase_avg_ns /= num_trials;
            final_results[i].total_avg_ns /= num_trials;
        }
        
        print_results(final_results, 3);
    } else {
        experiment_results_t final_result = { .system_name = get_system_name(config.system_type) };

        for (int trial = 0; trial < num_trials; trial++) {
            experiment_results_t trial_result = run_experiment(&config);
            final_result.map_phase_avg_ns += trial_result.map_phase_avg_ns;
            final_result.shuffle_phase_avg_ns += trial_result.shuffle_phase_avg_ns;
            final_result.reduce_phase_avg_ns += trial_result.reduce_phase_avg_ns;
            final_result.total_avg_ns += trial_result.total_avg_ns;
        }

        // Average the results
        final_result.map_phase_avg_ns /= num_trials;
        final_result.shuffle_phase_avg_ns /= num_trials;
        final_result.reduce_phase_avg_ns /= num_trials;
        final_result.total_avg_ns /= num_trials;

        print_results(&final_result, 1);
    }
    
    return 0;
}
