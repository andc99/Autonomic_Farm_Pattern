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

int main(){
	std::thread* thread_core;
//	thread_core = new std::thread(body, 1);
	int i = 0;
	Autonomic_Farm<int,int> aut_farm(body_2, 2, 6, true);
	aut_farm.run();
	for(int i = 0; i < 1000000000; i++){
		aut_farm.push(i);
	}
	aut_farm.push(EOS);
	
	int task;	
	while( (task = aut_farm.pop()) != EOS){
		//std::cout << "res: " << task << std::endl;
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

