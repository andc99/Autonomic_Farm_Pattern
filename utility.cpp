#include <thread>
#include <chrono>
#include <math.h>
#include <stdlib.h>
#include <iostream>
#include <algorithm>

#include <typeinfo>
//#include "worker.h"
#include "work_and_silent.cpp"
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
	for(int i = 0; i < n_tasks; i++){
		collection.push_back(i);
	}
	std::cout << sticky << std::endl;
	high_resolution_clock::time_point start_time = high_resolution_clock::now();
	Autonomic_Farm_Silent<int,int> afs(isPrime, n_contexts);
	for(int i : collection){
		afs.push(i);
	}
	afs.push(EOS);
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
	seq(collection);
	high_resolution_clock::time_point end_time_s = high_resolution_clock::now(); 
	std::cout << "Tseq: " << std::chrono::duration_cast<std::chrono::microseconds>(end_time_s - start_time_s).count() << std::endl;


	start_time_s = high_resolution_clock::now();
	std::thread a(seq, collection);
	a.join();
	end_time_s = high_resolution_clock::now(); 
	std::cout << "Tth: " << std::chrono::duration_cast<std::chrono::microseconds>(end_time_s - start_time_s).count() << std::endl;
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
	
	start_time_s = high_resolution_clock::now();
	std::vector<SafeQueue<int*>> que_in(1000000);
	std::vector<SafeQueue<int*>> que_out(1000000);
	SafeQueue<int*> que_emitter;
	SafeQueue<int*> que_collector;
	int eo = -1;
	for(int i : collection)
		que_emitter.safe_push(&i);
	que_emitter.safe_push(&eo);
	std::thread* emit = new std::thread([&] {
			int* task;
			int queue_id{0};
			while( *(task = que_emitter.safe_pop()) != eo){
				que_in[queue_id].safe_push(task);
				queue_id++;
				queue_id = queue_id%4;
			}
			for(int i = 0; i < 4; i++) //2
				que_in[i].safe_push(&eo);
			return;
			});

	std::thread* coll = new std::thread([&] {
			int* task;
			int queue_id{0};
			while( *(task = que_out[queue_id].safe_pop()) != eo){
				que_collector.safe_push(task);
				queue_id++;
				queue_id = queue_id%4;
			}
			return;
			});
	std::thread* w_1 =  new std::thread([&] {
			int* task;
			while( *(task = que_in[0].safe_pop()) != eo){
				int res = isPrime(*task);
				que_out[0].safe_push(&res);
			}
			que_out[0].safe_push(&eo);
			return;
			});
	std::thread* w_2 = new std::thread([&] {
			int* task;
			while( *(task = que_in[1].safe_pop()) != eo){
				int res = isPrime(*task);
				que_out[1].safe_push(&res);
			}
			que_out[1].safe_push(&eo);
			return;
			});
	std::thread* w_3 = new std::thread([&] {
			int* task;
			while( *(task = que_in[2].safe_pop()) != eo){
				int res = isPrime(*task);
				que_out[2].safe_push(&res);
			}
			que_out[2].safe_push(&eo);
			return;
			});
	std::thread* w_4 = new std::thread([&] {
			int* task;
			while( *(task = que_in[3].safe_pop()) != eo){
				int res = isPrime(*task);
				que_out[3].safe_push(&res);
			}
			que_out[3].safe_push(&eo);
			return;
			});
	coll->join();
	std::cout << "fine" << std::endl;
	end_time_s = high_resolution_clock::now(); 
	std::cout << "TPur: " << std::chrono::duration_cast<std::chrono::microseconds>(end_time_s - start_time_s).count() << std::endl;
	return 0;
}

