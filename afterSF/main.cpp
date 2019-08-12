#include <stdlib.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <math.h>
#include <vector>
#include <functional>

#include "autonomic_farm.h"

//service_time_farm() va dentro la farm

//sticky?, ff_buffer, collector, safe_queue
//timestamper
//ogni volta che creo la farm, deve prendere il massimo numero di contesti e fare in modulo quello 
//per assegnarlo ai core
//

//giustificare ubounded vs bounded queue
unsigned int isPrime(unsigned int x){
	if(x==2)
		return 1;
	if(x%2==0)
		return 0;
	int i = 2, sqr = sqrt(x);
	while(i <= sqr){
		if(x % i == 0)
			return 0;
		i++;
	}
	return 1;
}

template <class I, class O>
long parallel(unsigned int n_threads, std::function<I(O)> fun_body, bool sticky, std::vector<I>* collection){
	std::chrono::high_resolution_clock::time_point start_time = std::chrono::high_resolution_clock::now();
	Autonomic_Farm<I,O> afs(n_threads, fun_body, sticky, collection);
	afs.run_and_wait();
	std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();
	return std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
}

template <class T>
long sequential(std::vector<T>* collection){
	std::chrono::high_resolution_clock::time_point start_time = std::chrono::high_resolution_clock::now();
	for(T task : *collection){
		isPrime(task);
	}
	std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();
	return std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
}


int main(int argc, const char** argv){
	if(argc < 4){
		std::cout << "Missing Arguments.\nExample: " << argv[0] << " n_tasks n_threads sticky" << std::endl;
		return 1;
	}

	unsigned int n_tasks = atoi(argv[1]);
	unsigned int n_threads = atoi(argv[2]);
	bool sticky = atoi(argv[3]);

	long seq_time, par_time;
	std::vector<unsigned int> collection;
	std::cout << "---- Preparing Collection ----" << std::endl;
	for(unsigned int i = 1; i < n_tasks+1; i++)
		collection.push_back(i);
		//collection.push_back(std::numeric_limits<int>::max());


	std::cout << "---- Computing ----" << std::endl;
	par_time = parallel<unsigned int, unsigned int>(n_threads, isPrime, sticky, &collection);
	seq_time = sequential<unsigned int>(&collection);

	std::cout << "Par_TIME: " << par_time << std::endl;
	std::cout << "Seq_TIME: " << seq_time << std::endl;

	return 0;
}


