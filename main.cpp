#include <stdlib.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <math.h>
#include <vector>
#include <functional>

#include "autonomic_farm.h"

//service_time_farm() va dentro la farm
//nw_max perchè voglio evitare che possano crescere all'infinito le risorse utilizzate
//provato a vedere le differenze di performance tra il caso dove vengono allocati sopra a 4 thread esistentei altri 4 thread e le performance sono simili al caso in cui sono 4 thread e basta
//timestamper
//
//Enfatizzare: la mia è una ordere_farm perchè agisco sui puntatori delle posizioni quindi la collection uscente è ordinataaa!!
//
//A fare così con nw per capire quelli attivi ho il problema che il collector comunque ruota su tutte le code:
//caso estremo 64 thread di cui solo 1 attivo. Dovrà fare tante syscall. 
//Caso: Fa una pop su una coda che non ha nulla, si mette a dormire? --> SI serve una try_lock per forza.
//Facendo così sono sempre busy quindi molto costosa come operazione
//
//giustificare ubounded vs bounded queue
//size_t giustificare-->per generalizzare You may run on a system where size_t is 16 or 64 bits. It's size is implementation defined. Technically, it can be smaller than, equal to, or larger than an “unsigned int”. This allows our compiler to take the necessary steps for optimization purposes 
//fare il check nella farm che i valori non siano negativi
//
//
//E' definibile la formula per scalare con i tempi mettendo in realazione numero dei task, thread e code.
//Se ho 16 thread con code da 10 significa che andrebbero 160 elementi per saturare le code. Se metto 100 task ce ne sono 60 .... no non mi torna perchè comunque dovrebbe fare prima--- ed invece cambia mettendo 100 32 1000 1 vs 100 32 10 1
//calcolare dim iniziale buffer in funzione di quanti elementi ci sono nella collection
//
//
//la vera dfferenza con il lock free è che riamngono sempre busy e se ho 8 contesti più di 6 worker non li posso mettere altriementi si impallano alcuni (se metto emitter e collector su i primi due, forse non succede anche se ne dubito)
////dire che ci sono %hardware concurrency
//che è stata implementata in modo che fosse il più generale possibile
//dire che è possibile allacciare lo stream oppure no
//
//prendo il tempo con un rand per evitare degli eventuali pattern ricorrenti che mi sfasano il service time
//ed inotre, poichè le variabili del tempo sono in continuo aggiornamento ci sarebbe la ncessità di 
//gestire la race condition. Questo implica un aumento del tempo per processare ogni elemento.
//Quindi un sa
//Poiché non c'è una funzione che restituisce la cpu del thread chiamandola da un altro thread, sarebbe
//stato necessario aggiornare ad ogni ciclo di push e pop lo sched_get cpu chiamandlo quindi dal thread stesso e risolvendo il problema. Però così è necessaria la sincronizzazione e fare lock e unlock di continuo inutilmente. Per questo ho preferito addormentare su una variabile di condizione i thread. Inoltre il collector e l'emitter ho notato che avevano un service time piuttosto trascurabile quindi ci stava appesentirli con lock per la scansione del vettore di buffer
//con il modo del ne con lock è una rogna perche devo fare lock e unlock su quello ma inoltre, per evitare di mettere altre lock, avrei dovuto inserire una try_lock sul collector. In questo modo però avrei che diventa troppo intensive come operazione. 
//In quest'altro modo faccio un po' di lock e un lock ma posso fare hyperthreading
//Se i thread non sono sticky come faccio a garantire che quando sposto un thread su un core dove è presente già un thread, quest'ultimo thread non viene spostao dall'OS e che quindi aumento il parallelism degree involontariamente?
//--> Dire che ho voluto usare i thread sticky al fine di poter gestire e fare robe (anche loro con ff lo fanno, no)
//Passo la funzione di scegliere il nuovo elemento come parametro in modo da incapsulare le queue sulle quali deve inserire i valori o
//OGGI:
//rinominare queue in buffer
//GRAfici della pienezza delle queue
//
//RIFINITURE:
//rifinire makefile
//far in modo che sia agnostico al free o al lock buffer (roba del while !pop -> continue);
//mettere ssize_t che indica che può avere valori negativi
//incapsulare il movimento della queue in modo da poter definire una policy
//safe_try switch con safe_oush per il tempo
//impostare collector sì o no
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

long parallel(size_t n_threads, size_t n_max_threads, std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len, bool sticky, std::vector<ssize_t>* collection){
	std::chrono::high_resolution_clock::time_point start_time = std::chrono::high_resolution_clock::now();
	Autonomic_Farm afs(n_threads, n_max_threads, fun_body, buffer_len, sticky, collection);
	afs.run_and_wait();
	std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();
	return std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
}

long sequential(std::vector<ssize_t>* collection){
	std::chrono::high_resolution_clock::time_point start_time = std::chrono::high_resolution_clock::now();
	for(size_t i = 0; i < (*collection).size(); i++)
		(*collection)[i] = isPrime((*collection)[i]);
	std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();
	return std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
}


int main(int argc, const char** argv){
	if(argc < 6){
		std::cout << "Missing Arguments.\nExample: " << argv[0] << " n_tasks n_threads buffer_len sticky" << std::endl;
		return 1;
	}


	size_t n_tasks = std::stoul(argv[1]); //string to unsigned int
	size_t n_threads = std::stoul(argv[2]);
	size_t n_max_threads = std::stoul(argv[3]);
	size_t buffer_len = atoi(argv[4]);
	bool sticky = atoi(argv[5]);

	long seq_time, par_time;
	std::vector<ssize_t> collection_par, collection_seq;
	std::cout << "---- Preparing Collection ----" << std::endl;
	for(size_t i = 1; i < n_tasks+1; i++){
		collection_seq.push_back(std::numeric_limits<int>::max());
		collection_par.push_back(std::numeric_limits<int>::max());
	}

	std::cout << "---- Computing ----" << std::endl;
	par_time = parallel(n_threads, n_max_threads, isPrime, buffer_len, sticky, &collection_par);
	std::cout << "Par_TIME: " << par_time << std::endl;

	seq_time = sequential(&collection_seq);
	std::cout << "Seq_TIME: " << seq_time << std::endl;

/*	for(auto i : collection_par)
		std::cout << i << std::endl;
	
	std::cout << "----  ----" << std::endl;
	for(auto i : collection_seq)
		std::cout << i << std::endl;
*/
	std::cout << "Are Equal? " << (collection_par == collection_seq) << std::endl;

	return 0;
}

