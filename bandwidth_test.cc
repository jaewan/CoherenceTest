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
#include <emmintrin.h>

#define MEMORY_NODE 2
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
	return count/2; //This is to eliminate hyperthreads
}

// Function to be executed by each thread
void write_to_memory(int64_t* memory, size_t start, size_t end, int64_t value) {
	// Bind thread to NUMA node 0
	numa_run_on_node(0);
	int64_t* dst = (int64_t*)((int8_t*)memory + start);
	int64_t* end_dst = (int64_t*)((int8_t*)memory + end);
	while(dst<end_dst){
		*dst = value;
		dst++;
	}
}

void write_to_memory_nt(int64_t* memory, size_t start, size_t end, int64_t value) {
	// Bind thread to NUMA node 0
	numa_run_on_node(0);

	size_t size = end - start;
	void* dst = (uint8_t*)memory + start;
	void* src = &value;
	// Cast pointers to the appropriate type for 8-byte access
	long long* d = static_cast<long long*>(dst);
	const long long* s = static_cast<const long long*>(src);

	// Align the destination pointer to an 8-byte boundary
	size_t alignment = reinterpret_cast<uintptr_t>(d) & 0x7;
	if (alignment) {
		std::cout << "Memory is not aligned!" << std::endl;
		alignment = 8 - alignment;
		size_t copy_size = (alignment > size) ? size : alignment;
		std::memcpy(d, s, copy_size);
		d += copy_size / sizeof(uint64_t);
		s += copy_size / sizeof(uint64_t);
		size -= copy_size;
	}

	// Copy the bulk of the data using non-temporal stores (8 bytes at a time)
	size_t block_size = size / sizeof(value);
	for (size_t i = 0; i < block_size; ++i) {
		_mm_stream_si64(d, *s); // Copy 8 bytes (one long long)
		++d;
	}

	// Copy any remaining bytes using standard memcpy
	std::memcpy(d, s, size % 8);
}

void write_to_memory_nt_batch(int64_t* memory, size_t start, size_t end, int64_t value) {
	numa_run_on_node(0);
	size_t size = end - start;
	void* dst = (uint8_t*)memory + start;
	void* src = &value;
	// Cast the input pointers to the appropriate types
	uint8_t* d = static_cast<uint8_t*>(dst);
	const uint8_t* s = static_cast<const uint8_t*>(src);

	// Align the destination pointer to 16-byte boundary
	size_t alignment = reinterpret_cast<uintptr_t>(d) & 0xF;
	if (alignment) {
		alignment = 16 - alignment;
		size_t copy_size = (alignment > size) ? size : alignment;
		std::memcpy(d, s, copy_size);
		d += copy_size;
		//s += copy_size;
		size -= copy_size;
	}

	// Copy the bulk of the data using non-temporal stores
	size_t block_size = size / 64;
	for (size_t i = 0; i < block_size; ++i) {
		_mm_stream_si64(reinterpret_cast<long long*>(d), *reinterpret_cast<const long long*>(s));
		_mm_stream_si64(reinterpret_cast<long long*>(d + 8), *reinterpret_cast<const long long*>(s));
		_mm_stream_si64(reinterpret_cast<long long*>(d + 16), *reinterpret_cast<const long long*>(s));
		_mm_stream_si64(reinterpret_cast<long long*>(d + 24), *reinterpret_cast<const long long*>(s));
		_mm_stream_si64(reinterpret_cast<long long*>(d + 32), *reinterpret_cast<const long long*>(s));
		_mm_stream_si64(reinterpret_cast<long long*>(d + 40), *reinterpret_cast<const long long*>(s));
		_mm_stream_si64(reinterpret_cast<long long*>(d + 48), *reinterpret_cast<const long long*>(s));
		_mm_stream_si64(reinterpret_cast<long long*>(d + 56), *reinterpret_cast<const long long*>(s));
		d += 64;
	}

	// Copy the remaining data using standard memcpy
	std::memcpy(d, s, size % 64);
}

size_t atomic_operation(void *memory, int core_num, std::barrier *sync_point){
	size_t count = 0;
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core_num, &cpuset);
	if(sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0){
		std::cerr << "Error setting thread affinity: " << strerror(errno) << std::endl;
		return;
	}
	std::atomic<int64_t> *var = (std::atomic<int64_t>*)memory;

	sync_point->arrive_and_wait();
	auto start_time = std::chrono::steady_clock::now();
	auto end_time = start_time + std::chrono::seconds(3); // Run for 3 seconds

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
	size_t size = SIZE / sizeof(int64_t); // Number of integers
	int64_t* memory = static_cast<int64_t*>(numa_alloc_onnode(size * sizeof(int64_t), MEMORY_NODE));
	if (memory == nullptr) {
		std::cerr << "Failed to allocate memory on NUMA node 2." << std::endl;
		return 1;
	}

	// Clear allocated memory
	memset(memory, 0, size * sizeof(int64_t));

	// Get the number of CPUs on NUMA node 1
	int num_cpus = count_cpus_in_numa_node(1);
	if (num_cpus <= 0) {
		std::cerr << "No CPUs found on NUMA node 1." << std::endl;
		numa_free(memory, size * sizeof(int));
		return 1;
	}

	// Open a file to store the results
	std::string filename;
	switch(MEMORY_NODE){
		case 0:
			//filename = "results/local_ntbatchwrite_bandwidth.csv";
			break;
		case 1:
			//filename = "results/remote_ntbatchwrite_bandwidth.csv";
			break;
		case 2:
			//filename = "results/cxl_ntbatchwrite_bandwidth.csv";
			break;
	}
	std::ofstream results_file(filename);
	results_file << "threads,duration,bandwidth\n";

	// Test with an increasing number of threads
	for (size_t num_threads = 1; num_threads <= static_cast<size_t>(num_cpus); ++num_threads) {
		size_t chunk_size = size / num_threads;
		// To make all accesses within 8butes alignement
		size_t alignment = chunk_size&15;
		if(alignment){
			alignment = 16 - alignment;
			chunk_size += alignment;
		}
		std::vector<double> durations;
		std::vector<double> bandwidths;

		for (int trial = 0; trial < NUM_TRIALS; ++trial) {
			// Create and run threads
			std::vector<std::thread> threads;
			auto start_time = std::chrono::high_resolution_clock::now();
			std::barrier sync_point(num_threads);
			for (size_t i = 0; i < num_threads; ++i) {
				size_t start = i * chunk_size;
				size_t end = (i == num_threads - 1) ? size : (i + 1) * chunk_size;
				//threads.emplace_back(atomic_operation, memory, start, end, i + 1);
				threads.emplace_back(atomic_operation, memory, i+256, &sync_point);
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
