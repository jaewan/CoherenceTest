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

#define NUM_TRIALS 10
#define RUN_TIME 1

using MyBarrier = std::barrier<>;
int global_var_;
std::atomic<int> atomic_var_(0);

size_t local_increment(int core_num, MyBarrier& sync_point){
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

	int x = 0;
	size_t count = 0;
	while (std::chrono::steady_clock::now() < end_time) {
		asm volatile(
					"movl %0, %%eax\n"    // Load 'x' into the accumulator register
					"inc %%eax\n"         // Increment the value in the accumulator
					"movl %%eax, %0\n"    // Store the incremented value back to 'x'
					"mfence\n"             // Memory fence (see explanation below)
					: "+m" (x)             // 'x' is both input and output
					:
					: "eax", "memory"     // Clobber list: eax, memory
			);
			count++;
	}
	return count;
}

size_t global_increment(int core_num, MyBarrier& sync_point){
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

	size_t count = 0;
	while (std::chrono::steady_clock::now() < end_time) {
		asm volatile(
					"movl %0, %%eax\n"    
					"inc %%eax\n"         
					"movl %%eax, %0\n"    
					"mfence\n"           
					: "+m" (global_var_)
					:
					: "eax", "memory"  
			);
		count++;
	}
	return count;
}

size_t global_atomic(int core_num, MyBarrier& sync_point){
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

	size_t count = 0;
	while (std::chrono::steady_clock::now() < end_time) {
		atomic_var_.fetch_add(1);
		count++;
	}
	return count;
}

size_t local_atomic(int core_num, MyBarrier& sync_point){
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

	std::atomic<int> atomic_var(0);
	size_t count = 0;
	while (std::chrono::steady_clock::now() < end_time) {
		atomic_var.fetch_add(1);
		count++;
	}
	return count;
}

int main(int argc, char* argv[]) {
	cxxopts::Options options("Coherence Test", "Tests to see the effects of cache coherence");
	options.add_options()
		("f,false", "False Sharing")
		("i,interleave", "Distribute threads")
		("follower", "Follower Address and Port", cxxopts::value<std::string>())
		("m,memory_node", "Target Memory Node", cxxopts::value<int>()->default_value("0"))
		("r,record_result", "Store Results as csv", cxxopts::value<int>()->default_value("0"))
		;
	auto arguments = options.parse(argc, argv);

	// Open a file to store the results
	std::string filename = "results/coherence_atomic.csv";
	std::ofstream results_file(filename);
	if(arguments["record_result"].as<int>())
		results_file << "threads,local,global,diff\n";

	size_t num_cores = std::thread::hardware_concurrency()/4;
	num_cores = 256;
	int coreNumAdd = 128;
	for(int num_threads = 1; num_threads <= num_cores; num_threads++){
		double local_avg_ops = 0;
		for(int trial = 0; trial < NUM_TRIALS; trial++){
			double num_ops = 0;
			std::vector<std::future<size_t>> futs;
			MyBarrier sync_point(num_threads);
			for(int i = 0; i < num_threads; i++){
			/*
			  if(i%2)
				  futs.push_back(std::async(std::launch::async, local_increment, i + coreNumAdd, std::ref(sync_point)));
				else
				  futs.push_back(std::async(std::launch::async, local_increment, i, std::ref(sync_point)));
					*/
				futs.push_back(std::async(std::launch::async, local_atomic, i, std::ref(sync_point)));
			}
			for(auto &f:futs){
				num_ops += f.get();
			}
			local_avg_ops += num_ops;
		}
		local_avg_ops = local_avg_ops/(double)NUM_TRIALS;
		double global_avg_ops = 0;
		for(int trial = 0; trial < NUM_TRIALS; trial++){
			double num_ops = 0;
			std::vector<std::future<size_t>> futs;
			MyBarrier sync_point(num_threads);
			for(int i = 0; i < num_threads; i++){
			/*
				if(i%2)
				  futs.push_back(std::async(std::launch::async, global_increment, i + coreNumAdd, std::ref(sync_point)));
				else
				  futs.push_back(std::async(std::launch::async, global_increment, i, std::ref(sync_point)));
					*/
				futs.push_back(std::async(std::launch::async, global_atomic, i, std::ref(sync_point)));
			}
			for(auto &f:futs){
				num_ops += f.get();
			}
			global_avg_ops += num_ops;
		}
		global_avg_ops = global_avg_ops/(double)NUM_TRIALS;
		std::cout << "Threads: " << num_threads << ", Global Ops: " << global_avg_ops << " local:" << local_avg_ops << " = " << local_avg_ops / global_avg_ops <<std::endl;
		if(arguments["record_result"].as<int>())
			results_file << num_threads << "," << global_avg_ops << "," << local_avg_ops << "," << local_avg_ops / global_avg_ops << "\n";
	}

	// Close the results file
	if(arguments["record_result"].as<int>())
		results_file.close();

	return 0;
}
