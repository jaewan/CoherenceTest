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
#include <atomic>
#include <barrier>
#include <future>
#include <emmintrin.h>

#define MEMORY_NODE 0
#define NUM_TRIALS 5
#define RUN_TIME 3

using MyBarrier = std::barrier<>;

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
	return count/2; //This is to eliminate hyperthreads
}
size_t atomic_operation(void *memory, int core_num, MyBarrier& sync_point){
	size_t count = 0;
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core_num, &cpuset);
	if(sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0){
		std::cerr << "Error setting thread affinity: " << strerror(errno) << std::endl;
		return 0;
	}
	std::atomic<int64_t> *var = (std::atomic<int64_t>*)memory;

	sync_point.arrive_and_wait();
	auto start_time = std::chrono::steady_clock::now();
	auto end_time = start_time + std::chrono::seconds(RUN_TIME); 

	while (std::chrono::steady_clock::now() < end_time) {
		var->fetch_add(1);
		count++;
	}
	return count;
}

int main() {
	// Initialize NUMA library
	if (numa_available() == -1) {
		std::cerr << "NUMA is not available on this system." << std::endl;
		return 1;
	}

	// Allocate SIZE memory on CXM 
	int64_t* memory = static_cast<int64_t*>(numa_alloc_onnode(sizeof(int64_t), MEMORY_NODE));
	if (memory == nullptr) {
		std::cerr << "Failed to allocate memory on NUMA node 2." << std::endl;
		return 1;
	}

	// Clear allocated memory
	memset(memory, 0, sizeof(int64_t));

	// Get the number of CPUs on NUMA node 1
	int num_cpus = count_cpus_in_numa_node(1);
	if (num_cpus <= 0) {
		std::cerr << "No CPUs found on NUMA node 1." << std::endl;
		numa_free(memory, sizeof(int));
		return 1;
	}

	// Open a file to store the results
	std::string filename;
	switch(MEMORY_NODE){
		case 1:
			filename = "results/local_atomic.csv";
			break;
		case 0:
			filename = "results/remote_atomic.csv";
			break;
		case 2:
			//filename = "results/cxl_atomic.csv";
			filename = "results/cxl_remote_atomic.csv";
			break;
	}
	std::ofstream results_file(filename);
	results_file << "threads,num_ops\n";

	// Test with an increasing number of threads
	for (size_t num_threads = 1; num_threads <= static_cast<size_t>(num_cpus); ++num_threads) {
	//for (size_t num_threads = 128; num_threads <= static_cast<size_t>(num_cpus); ++num_threads) {
		double avg_ops = 0;

		for (int trial = 0; trial < NUM_TRIALS; ++trial) {
			double num_ops = 0;
			std::vector<std::future<size_t>> futs;
			MyBarrier sync_point(num_threads);
			for (size_t i = 0; i < num_threads; ++i) {
				futs.push_back(std::async(std::launch::async, atomic_operation, memory, i + 256, std::ref(sync_point)));
			}
			for(auto &f:futs){
				num_ops += f.get();
			}
			num_ops = num_ops/(double)num_threads;
			avg_ops += num_ops;
		}
		avg_ops = avg_ops/(double)NUM_TRIALS;

		// Store the results
		results_file << num_threads << "," << avg_ops << "\n";

		std::cout << "Threads: " << num_threads << ", Avg Ops: " << avg_ops << std::endl;
	}

	// Close the results file
	results_file.close();

	// Free the allocated memory
	numa_free(memory, sizeof(int));

	return 0;
}
