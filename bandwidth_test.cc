#include <numa.h>
#include <numaif.h>
#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <sched.h>
#include <chrono>
#include <fstream>
#include <numeric>

#define CXL_NODE 2
#define SIZE (1UL<<35)
#define NUM_TRIALS 5

// Function to count the number of CPUs in a CPU set
int count_cpus_in_numa_node(int node) {
	struct bitmask *cpumask = numa_allocate_cpumask();
	if (numa_node_to_cpus(node, cpumask) == -1) {
		std::cerr << "Failed to get CPUs for NUMA node " << node << std::endl;
		numa_free_cpumask(cpumask);
		return -1;
	}
	int count = 0;
	for (unsigned i = 0; i < cpumask->size; ++i) {
		if (numa_bitmask_isbitset(cpumask, i)) {
			++count;
		}
	}
	numa_free_cpumask(cpumask);
	return count;
}

// Function to be executed by each thread
void write_to_memory(int* memory, size_t start, size_t end, int value) {
	// Bind thread to NUMA node 0
	numa_run_on_node(0);
	for (size_t i = start; i < end; ++i) {
		memory[i] = value;
	}
}

int main() {
	// Initialize NUMA library
	if (numa_available() == -1) {
		std::cerr << "NUMA is not available on this system." << std::endl;
		return 1;
	}

	// Allocate SIZE memory on CXM 
	size_t size = SIZE / sizeof(int); // Number of integers
	int* memory = static_cast<int*>(numa_alloc_onnode(size * sizeof(int), CXL_NODE));
	if (memory == nullptr) {
		std::cerr << "Failed to allocate memory on NUMA node 2." << std::endl;
		return 1;
	}

	// Clear allocated memory
	memset(memory, 0, size * sizeof(int));

	// Get the number of CPUs on NUMA node 0
	int num_cpus = count_cpus_in_numa_node(0);
	if (num_cpus <= 0) {
		std::cerr << "No CPUs found on NUMA node 0." << std::endl;
		numa_free(memory, size * sizeof(int));
		return 1;
	}

	// Open a file to store the results
	std::ofstream results_file("results/cxl_write_bandwidth.csv");
	results_file << "threads,duration,bandwidth\n";

	// Test with an increasing number of threads
	for (size_t num_threads = 1; num_threads <= static_cast<size_t>(num_cpus); ++num_threads) {
		size_t chunk_size = size / num_threads;
		std::vector<double> durations;
		std::vector<double> bandwidths;

		for (int trial = 0; trial < NUM_TRIALS; ++trial) {
			// Create and run threads
			std::vector<std::thread> threads;
			auto start_time = std::chrono::high_resolution_clock::now();
			for (size_t i = 0; i < num_threads; ++i) {
				size_t start = i * chunk_size;
				size_t end = (i == num_threads - 1) ? size : (i + 1) * chunk_size;
				threads.emplace_back(write_to_memory, memory, start, end, i + 1);
			}

			// Wait for all threads to finish
			for (auto& th : threads) {
				th.join();
			}
			auto end_time = std::chrono::high_resolution_clock::now();

			// Calculate the duration and bandwidth
			std::chrono::duration<double> duration = end_time - start_time;
			double bandwidth = (size * sizeof(int)) / (1024.0 * 1024.0 * 1024.0) / duration.count(); // GB/s

			durations.push_back(duration.count());
			bandwidths.push_back(bandwidth);
		}

		// Calculate average duration and bandwidth
		double avg_duration = std::accumulate(durations.begin(), durations.end(), 0.0) / NUM_TRIALS;
		double avg_bandwidth = std::accumulate(bandwidths.begin(), bandwidths.end(), 0.0) / NUM_TRIALS;

		// Store the results
		results_file << num_threads << "," << avg_duration << "," << avg_bandwidth << "\n";

		std::cout << "Threads: " << num_threads << ", Avg Duration: " << avg_duration << " s, Avg Bandwidth: " << avg_bandwidth << " GB/s" << std::endl;
	}

	// Close the results file
	results_file.close();

	// Free the allocated memory
	numa_free(memory, size * sizeof(int));

	return 0;
}
