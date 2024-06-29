#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <sched.h>
#include <chrono>
#include <fstream>
#include <numeric>
#include <barrier>
#include <future>
#include <emmintrin.h>
#include <unistd.h>

#include <cxxopts.hpp> 

#define NUM_TRIALS 10
#define RUN_TIME 3

using MyBarrier = std::barrier<>;
int global_var_;

double calculateVariance(const std::vector<size_t>& values) {
    if (values.empty()) {
        return 0.0; // Handle empty vector
    }

    // Calculate the mean (average) - use double for accuracy
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    double mean = sum / values.size();

    // Calculate the sum of squared differences from the mean
    double sqDiffSum = 0.0;
    for (size_t value : values) {
        sqDiffSum += (static_cast<double>(value) - mean) * (static_cast<double>(value) - mean);
    }

    // Calculate the variance
    double variance = sqDiffSum / values.size();

    return variance;
}

size_t local_increment(int core_num, MyBarrier& sync_point){
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core_num, &cpuset);
	if(sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0){
		std::cerr << "Error setting thread affinity: " << strerror(errno) << std::endl;
		return 0;
	}

	/*
	cpu_set_t mask;
  pthread_t threadId = pthread_self();

  // Get the thread's affinity mask
  if (pthread_getaffinity_np(threadId, sizeof(cpu_set_t), &mask) != 0) {
    std::cerr << "Error getting thread affinity" << std::endl;
    return 0;
  }

  // Find and print the core IDs
  for (int i = 0; i < CPU_SETSIZE; i++) {
    if (CPU_ISSET(i, &mask)) {
      std::cout << core_num<< ":" << i << std::endl;
    }
  }
	unsigned int cpu_id,node_id;
	getcpu(&cpu_id, &node_id);
	std::cout << core_num << ":"<<cpu_id<<"="<<node_id<<std::endl;
	*/
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
	std::string filename = "results/softnuma.csv";
	std::ofstream results_file(filename);
	if(arguments["record_result"].as<int>())
		results_file << "coreNumAdd,local,local_var,global,global_var,diff\n";

	int num_rows = 11;
	int c[11][8] = {
	 {8,9,10,11,120,121,122,123}, //next soft numa ailgned
	 {8,9,10,11,124,125,126,127}, //next soft numa ailgned
	 {8,9,10,11,12,13,14,15}, //Same soft numa
	 {8,9,10,11,16,17,18,19}, //Next soft numa unaligned
	 {8,9,10,11,20,21,22,23}, //next soft numa ailgned
	 {8,9,10,11,128,129,130,131}, // Hard NUMA
	 {8,9,10,11,132,133,134,135}, // Hard NUMA
	 {8,16,24,32,40,48,56,64}, //All differente soft numa
	 {8,17,26,35,44,53,62,71},
	 {8,16,24,32,128,136,144,152},
	 {8,17,26,35,132,141,150,159}};
	 /*
	int num_rows = 8;
	int c[8][8] = {
	 {8,9,10,11,12,13,14,15}, //1 soft numa
	 {8,9,10,11,16,17,18,19}, //2
	 {8,9,10,16,17,18,24,25}, //3
	 {8,9,16,17,24,25,32,33}, //4
	 {8,16,17,24,25,32,33,40}, //5
	 {8,16,24,25,32,33,40,48}, //6
	 {8,16,24,32,33,40,48,56}, //7
	 {8,16,24,32,40,48,56,64} //8
	 };
	 */
	size_t num_cores = std::thread::hardware_concurrency()/4;
	num_cores = 1;
	int coreNumAdd = 0;
	std::vector<size_t> local_v;
	std::vector<size_t> global_v;
	for(int i=0; i<num_rows; i++){
	for(int num_threads = 1; num_threads <= num_cores; num_threads++){
		double local_avg_ops = 0;
		for(int trial = 0; trial < NUM_TRIALS; trial++){
			double num_ops = 0;
			std::vector<std::future<size_t>> futs;
			MyBarrier sync_point(num_threads);
			/*
			for(int i = 0; i < num_threads; i++){
			  if(i%2)
				  futs.push_back(std::async(std::launch::async, local_increment, i*2 + coreNumAdd, std::ref(sync_point)));
				else
				  futs.push_back(std::async(std::launch::async, local_increment, i*2, std::ref(sync_point)));
				futs.push_back(std::async(std::launch::async, local_increment, i*2, std::ref(sync_point)));
			}
					*/
			futs.push_back(std::async(std::launch::async, local_increment, c[i][0], std::ref(sync_point)));
			futs.push_back(std::async(std::launch::async, local_increment, c[i][1], std::ref(sync_point)));
			futs.push_back(std::async(std::launch::async, local_increment, c[i][2], std::ref(sync_point)));
			futs.push_back(std::async(std::launch::async, local_increment, c[i][3], std::ref(sync_point)));
			futs.push_back(std::async(std::launch::async, local_increment, c[i][4] + coreNumAdd, std::ref(sync_point)));
			futs.push_back(std::async(std::launch::async, local_increment, c[i][5] + coreNumAdd, std::ref(sync_point)));
			futs.push_back(std::async(std::launch::async, local_increment, c[i][6] + coreNumAdd, std::ref(sync_point)));
			futs.push_back(std::async(std::launch::async, local_increment, c[i][7] + coreNumAdd, std::ref(sync_point)));
			for(auto &f:futs){
				num_ops += f.get();
			}
			local_v.push_back(num_ops);
			local_avg_ops += num_ops;
		}
		local_avg_ops = local_avg_ops/(double)NUM_TRIALS;
		double global_avg_ops = 0;
		for(int trial = 0; trial < NUM_TRIALS; trial++){
			double num_ops = 0;
			std::vector<std::future<size_t>> futs;
			MyBarrier sync_point(num_threads);
			/*
			for(int i = 0; i < num_threads; i++){
				if(i%2)
				  futs.push_back(std::async(std::launch::async, global_increment, i*2 + coreNumAdd, std::ref(sync_point)));
				else
				  futs.push_back(std::async(std::launch::async, global_increment, i*2, std::ref(sync_point)));
				futs.push_back(std::async(std::launch::async, global_increment, i*2, std::ref(sync_point)));
			}
					*/
			futs.push_back(std::async(std::launch::async, global_increment, c[i][0] + coreNumAdd, std::ref(sync_point)));
			futs.push_back(std::async(std::launch::async, global_increment, c[i][1] + coreNumAdd, std::ref(sync_point)));
			futs.push_back(std::async(std::launch::async, global_increment, c[i][2] + coreNumAdd, std::ref(sync_point)));
			futs.push_back(std::async(std::launch::async, global_increment, c[i][3] + coreNumAdd, std::ref(sync_point)));
			futs.push_back(std::async(std::launch::async, global_increment, c[i][4] + coreNumAdd, std::ref(sync_point)));
			futs.push_back(std::async(std::launch::async, global_increment, c[i][5] + coreNumAdd, std::ref(sync_point)));
			futs.push_back(std::async(std::launch::async, global_increment, c[i][6] + coreNumAdd, std::ref(sync_point)));
			futs.push_back(std::async(std::launch::async, global_increment, c[i][7] + coreNumAdd, std::ref(sync_point)));
			for(auto &f:futs){
				num_ops += f.get();
			}
			global_v.push_back(num_ops);
			global_avg_ops += num_ops;
		}
		global_avg_ops = global_avg_ops/(double)NUM_TRIALS;
		std::cout << "Threads:["<<c[i][0]<<","<<c[i][1]<<","<<c[i][2]<<","<<c[i][3]<<"] [" << c[i][4] + coreNumAdd << "," << c[i][5]+coreNumAdd <<","<<c[i][6]+coreNumAdd<<","<< c[i][7]+coreNumAdd<<"]   Global Ops: " << global_avg_ops << " local:" << local_avg_ops << " = " << local_avg_ops / global_avg_ops <<std::endl;
		std::cout << "local var:" << calculateVariance(local_v) <<std::endl;
		std::cout << "global var:" << calculateVariance(global_v) <<std::endl;
		if(arguments["record_result"].as<int>())
			results_file << coreNumAdd << ","<< "," << local_avg_ops << ","<<calculateVariance(local_v) << ","  << global_avg_ops << "," << calculateVariance(global_v) <<","<< local_avg_ops / global_avg_ops << "\n";
			}
	}

	// Close the results file
	if(arguments["record_result"].as<int>())
		results_file.close();

	return 0;
}
