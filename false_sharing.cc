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
#include <unistd.h>

#include <cxxopts.hpp> 

#define NUM_TRIALS 5
#define NUM_ITERATIONS 10000000
#define RUN_TIME 3

using MyBarrier = std::barrier<>;

size_t writer(int64_t *memory, size_t idx, int core_num, MyBarrier& sync_point){
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core_num, &cpuset);
	if(sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0){
		std::cerr << "Error setting thread affinity: " << strerror(errno) << std::endl;
		return 0;
	}

	sync_point.arrive_and_wait();
	auto start_time = std::chrono::steady_clock::now();
	auto end_time = start_time + std::chrono::seconds(RUN_TIME); 

	while (std::chrono::steady_clock::now() < end_time) {
		memory[idx]++;
	}
	return (size_t)memory[idx];
	/*
	for(int i=0; i<NUM_ITERATIONS; i++){
		memory[idx]++;
	}
	*/
}

int main(int argc, char* argv[]) {
	cxxopts::Options options("Coherence Test", "Tests to see the effects of cache coherence");
	options.add_options()
		("f,false", "False Sharing")
		("i,interleave", "Distribute threads")
		("follower", "Follower Address and Port", cxxopts::value<std::string>())
		("m,memory_node", "Target Memory Node", cxxopts::value<int>()->default_value("0"))
		;
	auto arguments = options.parse(argc, argv);

	int MEMORY_NODE = arguments["memory_node"].as<int>();

	// Initialize NUMA library
	if (numa_available() == -1) {
		std::cerr << "NUMA is not available on this system." << std::endl;
		return 1;
	}

	// Allocate SIZE memory
	unsigned int num_cores = std::thread::hardware_concurrency();
	long cacheLineSize = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
	int64_t* false_sharing_memory = static_cast<int64_t*>(numa_alloc_onnode(num_cores * sizeof(int64_t), MEMORY_NODE));
	int64_t* memory = static_cast<int64_t*>(numa_alloc_onnode(num_cores * cacheLineSize * sizeof(int64_t), MEMORY_NODE));
	if (memory == nullptr || false_sharing_memory == nullptr) {
		std::cerr << "Failed to allocate memory on NUMA node " << MEMORY_NODE << std::endl;
		return 1;
	}

	// Clear allocated memory
	memset(false_sharing_memory, 0, num_cores * sizeof(int64_t));
	memset(memory, 0, num_cores * cacheLineSize * sizeof(int64_t));

/*
	// Open a file to store the results
	std::string filename;
	switch(MEMORY_NODE){
	case 1:
		filename = "results/local_falseshare_distribute.csv";
		break;
	case 0:
		filename = "results/remote_falseshare.csv";
		break;
	case 2:
		filename = "results/cxl_falseshare.csv";
		break;
	}
	std::ofstream results_file(filename);
	results_file << "threads,false_share,non_share,diff\n";

	num_cores = 128;
	for(int num_threads = 1; num_threads <= num_cores; num_threads++){
		double non_avg_ops = 0;
		for(int trial = 0; trial < NUM_TRIALS; trial++){
			double num_ops = 0;
			std::vector<std::future<size_t>> futs;
			MyBarrier sync_point(num_threads);
			for(int i = 0; i < num_threads; i++){
			  if(i%2)
					futs.push_back(std::async(std::launch::async, writer, memory, i*(cacheLineSize/sizeof(int64_t)), i + 128, std::ref(sync_point)));
				else
					futs.push_back(std::async(std::launch::async, writer, memory, i*(cacheLineSize/sizeof(int64_t)), i, std::ref(sync_point)));
			}
			for(auto &f:futs){
				num_ops += f.get();
			}
			non_avg_ops += num_ops;
		}
		non_avg_ops = non_avg_ops/(double)NUM_TRIALS;
		double false_avg_ops = 0;
		for(int trial = 0; trial < NUM_TRIALS; trial++){
			double num_ops = 0;
			std::vector<std::future<size_t>> futs;
			MyBarrier sync_point(num_threads);
			for(int i = 0; i < num_threads; i++){
				if(i%2)
					futs.push_back(std::async(std::launch::async, writer, false_sharing_memory, i, i + 128, std::ref(sync_point)));
				else
					futs.push_back(std::async(std::launch::async, writer, false_sharing_memory, i, i, std::ref(sync_point)));
			}
			for(auto &f:futs){
				num_ops += f.get();
			}
			false_avg_ops += num_ops;
		}
		false_avg_ops = false_avg_ops/(double)NUM_TRIALS;
		std::cout << "Threads: " << num_threads << ", false sharing Ops: " << false_avg_ops << " non shared:" << non_avg_ops <<std::endl;
		results_file << num_threads << "," << false_avg_ops << "," << non_avg_ops << "," << false_avg_ops / non_avg_ops << "\n";
	}

	// Close the results file
	results_file.close();
	*/
	double false_avg_ops = 0;
	double non_avg_ops = 0;
	for (int trial = 0; trial < NUM_TRIALS; trial++){
		double num_ops = 0;
		std::vector<std::future<size_t>> futs;
		MyBarrier sync_point(8);
		/*
		for(int i=0; i<8; i++)
			futs.push_back(std::async(std::launch::async, writer, memory, i*(cacheLineSize/sizeof(int64_t)), i + 128, std::ref(sync_point)));
			*/
		for(int i=0; i<4; i++)
			futs.push_back(std::async(std::launch::async, writer, memory, i*(cacheLineSize/sizeof(int64_t)), i + 128, std::ref(sync_point)));
		for(int i=0; i<4; i++)
			futs.push_back(std::async(std::launch::async, writer, memory, i*(cacheLineSize/sizeof(int64_t)), i , std::ref(sync_point)));
		for(auto &f : futs){
			non_avg_ops += f.get();
		}
	}
	non_avg_ops = non_avg_ops/(double)NUM_TRIALS;

	for (int trial = 0; trial < NUM_TRIALS; trial++){
		double num_ops = 0;
		std::vector<std::future<size_t>> futs;
		MyBarrier sync_point(8);
		for(int i=0; i<4; i++)
			futs.push_back(std::async(std::launch::async, writer, false_sharing_memory, i*(cacheLineSize/sizeof(int64_t)), i + 128, std::ref(sync_point)));
		for(int i=0; i<4; i++)
			futs.push_back(std::async(std::launch::async, writer, false_sharing_memory, i*(cacheLineSize/sizeof(int64_t)), i , std::ref(sync_point)));
		/*
		for(int i=0; i<8; i++)
			futs.push_back(std::async(std::launch::async, writer, false_sharing_memory, i*(cacheLineSize/sizeof(int64_t)), i + 128, std::ref(sync_point)));
			*/
		for(auto &f : futs){
			false_avg_ops += f.get();
		}
	}
	false_avg_ops = false_avg_ops/(double)NUM_TRIALS;

	std::cout << 8 << "," << false_avg_ops << "," << non_avg_ops << "," << non_avg_ops/false_avg_ops << "\n";

	// Free the allocated memory
	numa_free(memory, num_cores * cacheLineSize *  sizeof(int64_t));
	numa_free(false_sharing_memory, num_cores * sizeof(int64_t));

	return 0;
}
