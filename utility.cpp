#include <thread>
#include <chrono>
#include <math.h>
#include <stdlib.h>
#include <iostream>
#include <algorithm>

#include <typeinfo>
//#include "worker.h"
//#include "work_and_silent.cpp"
#include "work_alone.h"
//#include "free_autonomic.h" 

#define EOS -1

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

long isPrime_stupid(long x){
	long i = 2;
	while(i <= x){
		if(x % i == 0)
			return 0;
		i++;
	}
	return 1;
}

int sleepy(int x){
	long time = 0, counter;
	high_resolution_clock::time_point start_time = high_resolution_clock::now();
	while(time < x){
		counter++;	
		std::this_thread::sleep_for(std::chrono::milliseconds(x));
		high_resolution_clock::time_point end_time = high_resolution_clock::now();
		time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
	}
	return counter;
}

void seq(std::vector<int> collection){
	int pos = 0;
	for(int i : collection){
	 	collection[pos] = isPrime(i);
		pos++;
	}
	return;
}


int main(int argc, const char** argv){
	int n_tasks = atoi(argv[1]);	
	int n_contexts = atoi(argv[2]);
	bool sticky = atoi(argv[3]);
	std::vector<int> collection;
	for(int i = n_tasks; i < n_tasks+n_tasks; i++){
		collection.push_back(std::numeric_limits<int>::max());
	}
	//collection.push_back(EOS); c'è già nell'emitte c'è già nell'emitterr
	std::cout << sticky << std::endl;
	high_resolution_clock::time_point start_time = high_resolution_clock::now();
	Autonomic_Farm_Silent<int,int> afs(isPrime, n_contexts, &collection);
//	for(int i : collection){
//		afs.push(i);
//	}
//	afs.push(EOS);
	afs.run_and_wait();
	//Autonomic_Farm<int,int> aut_farm(&collection, isPrime, n_contexts, 6, sticky);
	//aut_farm.run_and_wait();
	//non d'accordo che poppi anche
	int task;
	//while( (task = afs.pop()) != EOS){
	//	std::cout << "res: " << task << std::endl;
	//}
	high_resolution_clock::time_point end_time = high_resolution_clock::now();                
	std::cout << "Tpar: " << std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count() << std::endl;

	high_resolution_clock::time_point start_time_s = high_resolution_clock::now();
	//seq(collection);
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
	
	return 0;
}

