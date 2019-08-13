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
//size_t giustificare-->per generalizzare You may run on a system where size_t is 16 or 64 bits. It's size is implementation defined. Technically, it can be smaller than, equal to, or larger than an “unsigned int”. This allows our compiler to take the necessary steps for optimization purposes 
//fare il check nella farm che i valori non siano negativi
//
//
//E' definibile la formula per scalare con i tempi mettendo in realazione numero dei task, thread e code.
//Se ho 16 thread con code da 10 significa che andrebbero 160 elementi per saturare le code. Se metto 100 task ce ne sono 60 .... no non mi torna perchè comunque dovrebbe fare prima--- ed invece cambia mettendo 100 32 1000 1 vs 100 32 10 1
//calcolare dim iniziale buffer in funzione di quanti elementi ci sono nella collection
int isPrime(int x){
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
    //	std::this_thread::sleep_for (std::chrono::seconds(1));
	return 1;
}

long parallel(size_t n_threads, std::function<size_t(size_t)> fun_body, size_t buffer_len, bool sticky, std::vector<size_t>* collection){
	std::chrono::high_resolution_clock::time_point start_time = std::chrono::high_resolution_clock::now();
	Autonomic_Farm afs(n_threads, fun_body, buffer_len, sticky, collection);
	afs.run_and_wait();
	std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();
	return std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
}

long sequential(std::vector<size_t>* collection){
	std::chrono::high_resolution_clock::time_point start_time = std::chrono::high_resolution_clock::now();
	for(size_t i = 0; i < (*collection).size(); i++)
		(*collection)[i] = isPrime((*collection)[i]);
	std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();
	return std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
}


int main(int argc, const char** argv){
	if(argc < 5){
		std::cout << "Missing Arguments.\nExample: " << argv[0] << " n_tasks n_threads buffer_len sticky" << std::endl;
		return 1;
	}

	size_t n_tasks = std::stoul(argv[1]); //string to unsigned int
	size_t n_threads = std::stoul(argv[2]);
	std::cout << n_tasks << " " << n_threads << std::endl;
	size_t buffer_len = atoi(argv[3]);
	bool sticky = atoi(argv[4]);

	long seq_time, par_time;
	std::vector<size_t> collection_par, collection_seq;
	std::cout << "---- Preparing Collection ----" << std::endl;
	for(size_t i = 1; i < n_tasks+1; i++){
		collection_seq.push_back(std::numeric_limits<int>::max());
		collection_par.push_back(std::numeric_limits<int>::max());
	}

	std::cout << "---- Computing ----" << std::endl;
	par_time = parallel(n_threads, isPrime, buffer_len, sticky, &collection_par);
	
	seq_time = sequential(&collection_seq);

/*	for(auto i : collection_par)
		std::cout << i << std::endl;
	
	std::cout << "----  ----" << std::endl;
	for(auto i : collection_seq)
		std::cout << i << std::endl;
*/
	std::cout << "Are Equal? " << (collection_par == collection_seq) << std::endl;
	std::cout << "Par_TIME: " << par_time << std::endl;
	std::cout << "Seq_TIME: " << seq_time << std::endl;

	return 0;
}


