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

void body(int id_thread){ //non una classe per evitare di appesantire 
	std::cout << "thread started" << std::endl;
	while(true){
		std::cout << "Thread #" << id_thread << ": on CPU " << sched_getcpu() << "\n";
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
}

int move_thread(int id_core, std::thread* thread){
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(id_core, &cpuset);
	int error = pthread_setaffinity_np((*thread).native_handle(), sizeof(cpu_set_t), &cpuset);
	if (error != 0)
		std::cerr << "Error calling pthread_setaffinity_np: " << error << "\n";
	return error;
}

int main(){
	std::thread* thread_core;
//	thread_core = new std::thread(body, 1);
	int i = 0;
	SafeQueue<int>* in;
	SafeQueue<int>* out;
	Autonomic_Farm<int,int> aut_farm(body_2, 4, 4);
	aut_farm.run();
	for(int i = 1; i < 10; i++){
		aut_farm.push(i);
	}
	aut_farm.push(EOS);
	
	for(int i = 1; i < 10; i++){
		std::cout << "--> remove: " << aut_farm.pop() << std::endl;
	}
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

