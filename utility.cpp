#include <thread>
#include <chrono>
#include <math.h>
#include <stdlib.h>
#include <iostream>
#include <algorithm>

#include <typeinfo>
#include "worker.h"

using namespace std::chrono;

//std::chrono::high_resolution_clock::now()

long service_time_farm(long emitter, long collector, long worker, int nw){
	return std::max({emitter, collector, worker/nw});
}

int body_2(int task){ //non una classe per evitare di appesantire 
	task++;
	return task;
}


int isPrime(int x){
	if(x == 2)
		return 1;
	if(x % 2 == 0){
		return 0;}
	int i = 2;
	int sqR = sqrt(x);
	while(i <= sqR){
		if(x % i == 0)
			return 0;
		i++;
	}
	return 1;
}

void seq(std::vector<int> collection){
	for(int i : collection){
	 	collection[i] = isPrime(i);
	}
	return;
}


int main(int argc, const char** argv){
	int n_tasks = atoi(argv[1]);	
	int n_contexts = atoi(argv[2]);
	bool sticky = atoi(argv[3]);	
	std::vector<int> collection;
	for(int i = 0; i < n_tasks; i++){
		collection.push_back(i);
	}
	std::cout << sticky << std::endl;
	high_resolution_clock::time_point start_time = high_resolution_clock::now();
	Autonomic_Farm<int,int> aut_farm(&collection, isPrime, n_contexts, 6, sticky);
	aut_farm.run_and_wait();
	int task;	
	//non d'accordo che poppi anche
	//while( (task = aut_farm.pop()) != EOS){
		//std::cout << "res: " << task << std::endl;
	//}
	high_resolution_clock::time_point end_time = high_resolution_clock::now();                
	std::cout << "Tpar: " << std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count() << std::endl;

	high_resolution_clock::time_point start_time_s = high_resolution_clock::now();
	std::cout << "----" << std::endl;
	seq(collection);
	high_resolution_clock::time_point end_time_s = high_resolution_clock::now(); 
	std::cout << "Tseq: " << std::chrono::duration_cast<std::chrono::microseconds>(end_time_s - start_time_s).count() << std::endl;
	
//	std::cout << "Start" << std::endl;
//	while(true){
//		std::cout << "run " << std::endl;
//		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
//		move_thread(i%8, thread_core);
//		i++;
//	}
//	high_resolution_clock::time_point start_time = high_resolution_clock::now();                
//	high_resolution_clock::time_point end_time = high_resolution_clock::now();                
//	std::cout << std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count() << std::endl;
//	long variable =	std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
//	std::cout << typeid(variable).name() << std::endl;
//	std::cout << variable << std::endl;
//	std::cout << service_time_farm(3,4,5,2) << std::endl;
}

