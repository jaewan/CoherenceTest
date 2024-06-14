#include <numa.h>
#include <numaif.h>
#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <sched.h>
#include <chrono>

#define CXL_NODE 1
#define SIZE (1UL<<35)

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

int allocate_remote_node() {
	// Initialize NUMA library
	if (numa_available() == -1) {
		std::cerr << "NUMA is not available on this system." << std::endl;
		return 1;
	}

	// Allocate 1 MB of memory on NUMA node 0
	size_t size = 1024 * 1024; // 1 MB
	void *memory = numa_alloc_onnode(size, 0);
	if (memory == nullptr) {
		std::cerr << "Failed to allocate memory on NUMA node 0." << std::endl;
		return 1;
	}

	// Use the memory (example: write and read data)
	memset(memory, 0, size); // Initialize memory to zero
	int *int_memory = static_cast<int*>(memory);
	int_memory[0] = 42; // Write a value
	std::cout << "Value at the first integer of allocated memory: " << int_memory[0] << std::endl;

	// Free the allocated memory
	numa_free(memory, size);

	return 0;
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
		std::cerr << "Failed to allocate memory on CXL " << std::endl;
		return 1;
	}

	// Clear allocated memory
	memset(memory, 0, SIZE);

	// Number of threads to create
	size_t num_threads = count_cpus_in_numa_node(0);
	if (num_threads <= 0) {
        std::cerr << "No CPUs found on NUMA node 0." << std::endl;
        numa_free(memory, size * sizeof(int));
        return 1;
	}
	size_t chunk_size = size / num_threads;

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

	std::cout << "Write bandwidth: " << bandwidth << " GB/s" << std::endl;


	// Free the allocated memory
	numa_free(memory, size * sizeof(int));

	return 0;
}
