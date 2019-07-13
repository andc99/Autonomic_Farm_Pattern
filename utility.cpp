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

std::vector<int> seq(){
	std::vector<int> vec;
	for(int i = 0; i < 10000; i++){
	 	vec.push_back(isPrime(i));
	}
	return vec;
}


int main(){
	high_resolution_clock::time_point start_time = high_resolution_clock::now();
	Autonomic_Farm<int,int> aut_farm(isPrime, 2, 6, true);
	aut_farm.run();
	for(int i = 0; i < 100000; i++){
		aut_farm.push(i);
	}
	aut_farm.push(EOS);
	int task;	
	while( (task = aut_farm.pop()) != EOS){
		//std::cout << "res: " << task << std::endl;
	}
	high_resolution_clock::time_point end_time = high_resolution_clock::now();                
	std::cout << "Tpar: " << std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count() << std::endl;

	high_resolution_clock::time_point start_time_s = high_resolution_clock::now();
	std::cout << "----" << std::endl;
	seq();
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

