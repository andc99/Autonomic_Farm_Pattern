#include <stdlib.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <math.h>
#include <vector>
#include <functional>

#include "autonomic_farm.hpp"

// Some Test Functions
//
/////////////////////////////////////////////
//Suggested: t1 = 2147483629, t2 = 53687090, t3 = 4294967291;

size_t isPrime(size_t x){
	if(x==2)
		return 1;
	if(x%2==0)
		return 0;
	size_t i = 2, sq = sqrt(x);
	while(i <= sq){
		if(x % i == 0)
			return 0;
		i++;
	}	
	return 1;
}

//////////////////////////////////////////////
//Suggested: t1 = 400, t2 = 100, t3 = 800;
int busy_wait(size_t time){
	size_t act = 0;
	std::chrono::high_resolution_clock::time_point start_time = std::chrono::high_resolution_clock::now();
	while(act <= time){
		std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();
		act = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
		continue;
	}
	return 0;
}

//////////////////////////////////////////////
int fib(int x){
	if(x==1 || x==0)
		return(x);
	else
		return fib(x-1) + fib(x-2);
}
//////////////////////////////////////////////


//Compute the parallel implementation
long parallel(long ts_goal, size_t n_threads, size_t n_max_threads, std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len, std::vector<size_t>* collection, long sliding_size){
	std::chrono::high_resolution_clock::time_point start_time = std::chrono::high_resolution_clock::now();
	Autonomic_Farm afs(ts_goal, n_threads, n_max_threads, fun_body, buffer_len, collection, sliding_size);
	afs.run();
	size_t a;
	while((a = afs.pop_outputs()) != -1){} //Pop element from collector's buffer
	afs.join();
	std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();
	return std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
}

//Compute the sequential Implmentation
long sequential(std::function<ssize_t(ssize_t)> fun_body, std::vector<size_t>* collection){
	std::chrono::high_resolution_clock::time_point start_time = std::chrono::high_resolution_clock::now();
	for(size_t i = 0; i < (*collection).size(); i++)
		(*collection)[i] = fun_body((*collection)[i]); 
	std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();
	return std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
}


int main(int argc, const char** argv){
	if(argc < 9){
		std::cout << "Missing Arguments.\nExample: " << argv[0] << " n_tasks start_degree max_degree buffer_len ts_goal sliding_size task_1 task_2 task_3" << std::endl;
		return 1;
	}


	size_t n_tasks = std::stoul(argv[1]); 
	size_t n_threads = std::stoul(argv[2]);
	size_t n_max_threads = std::stoul(argv[3]);
	size_t buffer_len = atoi(argv[4]);
	long ts_goal = atoi(argv[5]);
	long sliding_size = atoi(argv[6]);
	size_t task_1 = std::stoul(argv[7]);
	size_t task_2 = std::stoul(argv[8]);
	size_t task_3 = std::stoul(argv[9]);


	long seq_time, par_time;
	std::vector<size_t> collection_par, collection_seq;
	std::cout << "---- Preparing Collection ----" << std::endl;

 
	
	for(size_t i = 0; i < n_tasks/3; i++){
//		collection_seq.push_back(task_1);
		collection_par.push_back(task_1);
	}
	for(size_t i = n_tasks/3; i < n_tasks*2/3; i++){
//		collection_seq.push_back(task_2);
		collection_par.push_back(task_2);
	}
	for(size_t i = n_tasks*2/3; i < n_tasks; i++){
//		collection_seq.push_back(task_3);
		collection_par.push_back(task_3);
	}
	
	std::cout << "---- Computing ----" << std::endl;

	//runs parallel
	par_time = parallel(ts_goal, n_threads, n_max_threads, busy_wait, buffer_len, &collection_par, sliding_size);
	std::cout << "Par_TIME: " << par_time << std::endl;

	//runs sequential
//	seq_time = sequential(busy_wait, &collection_seq);
//	std::cout << "Seq_TIME: " << seq_time << std::endl;

//	std::cout << "Scalability " << (float) seq_time/par_time << std::endl;
//	std::cout << "Are Equal? " << (collection_par == collection_seq) << std::endl;
	return 0;
}


