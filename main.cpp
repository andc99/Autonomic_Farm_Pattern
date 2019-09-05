#include <stdlib.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <math.h>
#include <vector>
#include <functional>

#include "autonomic_farm.h"


//OGGI
//mettere che se raddoppia i context e non c'è un grande vantaggio, allora significa che non era necessario.
//le ho messe bounded per controllare il bottleneck
//capire se ProcessingElement farli tornare Worker
//-voglio stare il più vicino al tsgoal perché se act_ts > allora devo aumentare i worker. Se act_ts < ts_goal allora singnifica che sto andando più veloce è magari sto sprecando risurse
//-potremmo non considerare il bottleneck per aumentare ma nel caso avessimo uno stream non definito, un bottleneck potrebbe andare a restituire un errore nel momento in cui il numro degli elementi in coda supera il bound, quindi il check del bottleneck di per sè ovvia a questo problema ed indirettamente a quello del degree per il Ts
//- discutibile mettere un botto di thread attivi per core ma quello dipende principalmente dal tipo di processore
//più è veloce, meglio li regge quindi max_nw è il vero valore su cui deve stare attento uno quando utilizza il pattern
//-nw deve essere 8 e trasferire i restanti su nw_max es: nw = 10 e hw = 8 e max_nw 11 --> nw = 8 e max_nw = 13? oppure mettere che massimo arriva a 11 e nw viene buttato su 8. La seconda mi sembra più senso perchè l'utente chiede di usare al massimo 11 thread
//-il mio caso è diverso perchè comunque alloco, l'unica differenza è che se ho hw = 8 e metto nw = 3 allora 5 mi vanno neglio idle
//-acneh il manager potrebe muoversi altrimenti potrebbero attaccare quel core col manager e rallentare tutto
//-struttura dati per lo stability problem
//-manager a parte ma comunque su un core dove c'è emitter o collector, con possibilità di metterlo a parte
//-try-pop sul collector?
//-possibilità di rimuovere il collector
//-se non viene rimosso allora c'è da specificare un body perchè altreimenti non avrebbe veramente senso, la collection è già modificat
//-enache emitter deve poter decidere di esporre la coda oppure no
//-pulire variabili inutili
//-pulire tipizzazioni stupide
//-throughput: se la media dei ST di un worker si discosta molto dagli altri allora altra app sopra, quindi sposto
//quello sopra deve essere controllato solo sui nw worker!!
//-dire che più nw_max non ha senso perchè già dopo il numero di core siamo in hyperthreading
//non troppo sicuro di quello scritto sopra, perchè quando runno, metto 8 worker ma ho già altri 2 che sono collector ed emitter quindi 10. Dicimao che nell'implmentazione, se ne metto più, prendo dagli idle
//
//Pro-active soluzione
//occhio all'hyperthreading, perchè mettere tutti sullo stesso contesto non è furbo, mettere su due contesti distinti
//poichè i contesti dell'architettura di riferimento sono 2 e ripartono da 0 al 128, allora..
//Rapporto user time e sys time al variare della lunghezza dell'input. Con 10000, rapporto 149, cin 1000000, rapporto 102  (tutto con 2 thread) 
//ho provato a mettere 64 thread su un unico core e farlo runnare. Il tempo è maggiore del sequenziale (ok) e non è distante dall'averlo fatto runnare solo con 1 thread (sequenziale overheadato parallelo)
//Mostrarlo tramite grafici. Easy
//processed_elements dovrebbero essere size_t (o unsigned long) non long (sia in farm che in buffer.h)
//potrei far fare all'emitter ed al collector il manager. Quando finisce l'Emitter di inviare tutti gli elementi, smette anche il manager. Non male come soluzione considerando che non fa molto l'emitter. Poi però c'è da controllare quanto varia il suo service time. Qui viene però una formula: l'emitter definisce il lower bound di rate ache cui manda elementi. Quindi se l'emitter ci metto 60 per ogni push ed ho 4 worker, ad ogni worker arriva lavoro dopo 180, quindi se il task dura 180 o più è buono, altrimenti no. FORMULA: TsEmitter*(nw-1) <= TsWorker. Se Emitter è 300 e nw = 4 dopo 900 arriva il prossimo task e quindi affinchè sia performante TsWorker >= TsEmitter*(nw-1). Il caso estremo dove non c'è parallelismo è quello in cui ad ogni istante viene calcolato solo 1 elemento, quindi quadno TsWorker è << TsEmitter.
//uindi presumo che su un core dove c'è un thread con try_pop, può non fare un buon hyperthreading
//
//farm oridnata
//
//timestamper
//La lunghezza della code è in funzione ad una formula (disegno) no, mi basta la gestione dei bottleneck
//Enfatizzare: la mia è una ordere_farm perchè agisco sui puntatori delle posizioni quindi la collection uscente è ordinataaa!!
//Differenza tra con e senza collector, inesistente. La vera differenza si potrebbe avere quando i worker hanno task moolto leggeri
//3647804 vs 3647720
//
//A fare così con nw per capire quelli attivi ho il problema che il collector comunque ruota su tutte le code:
//caso estremo 64 thread di cui solo 1 attivo. Dovrà fare tante syscall. 
//Caso: Fa una pop su una coda che non ha nulla, si mette a dormire? --> SI serve una try_lock per forza.
//Facendo così sono sempre busy quindi molto costosa come operazione
//
//giustificare ubounded vs bounded queue
//size_t giustificare-->per generalizzare You may run on a system where size_t is 16 or 64 bits. It's size is implementation defined. Technically, it can be smaller than, equal to, or larger than an “unsigned int”. This allows our compiler to take the necessary steps for optimization purposes 
//
//
//E' definibile la formula per scalare con i tempi mettendo in realazione numero dei task, thread e code.
//Se ho 16 thread con code da 10 significa che andrebbero 160 elementi per saturare le code. Se metto 100 task ce ne sono 60 .... no non mi torna perchè comunque dovrebbe fare prima--- ed invece cambia mettendo 100 32 1000 1 vs 100 32 10 1
//
//
//la vera dfferenza con il lock free è che riamngono sempre busy e se ho 8 contesti più di 6 worker non li posso mettere altriementi si impallano alcuni (se metto emitter e collector su i primi due, forse non succede anche se ne dubito)
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
//safe_try switch con safe_oush per il tempo
//impostare collector sì o no
//
int isPrime(size_t x){
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

int fib(int x){
	if(x==1 || x==0)
		return(x);
	else
		return fib(x-1) + fib(x-2);
}

int sleep(size_t x){
    	std::this_thread::sleep_for (std::chrono::milliseconds(x));
	return 1;
}

long parallel(long ts_goal, size_t n_threads, size_t n_max_threads, std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len, std::vector<ssize_t>* collection, long sliding_size){
	std::chrono::high_resolution_clock::time_point start_time = std::chrono::high_resolution_clock::now();
	Autonomic_Farm afs(ts_goal, n_threads, n_max_threads, fun_body, buffer_len, collection, sliding_size);
//	afs.run_and_wait();
	afs.run();
	size_t a;
	while((a = afs.pop_outputs()) != -1){
		//std::cout << a << std::endl;
	}
	afs.join();
	std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();
	return std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
}

long sequential(std::function<ssize_t(ssize_t)> fun_body, std::vector<ssize_t>* collection){
	std::chrono::high_resolution_clock::time_point start_time = std::chrono::high_resolution_clock::now();
	for(size_t i = 0; i < (*collection).size(); i++)
		(*collection)[i] = fun_body((*collection)[i]);
	std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();
	return std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
}


int main(int argc, const char** argv){
	if(argc < 7){
		std::cout << "Missing Arguments.\nExample: " << argv[0] << " n_tasks n_threads buffer_len sticky" << std::endl;
		return 1;
	}


	size_t n_tasks = std::stoul(argv[1]); //string to unsigned int
	size_t n_threads = std::stoul(argv[2]);
	size_t n_max_threads = std::stoul(argv[3]);
	size_t buffer_len = atoi(argv[4]);
	long ts_goal = atoi(argv[5]);
	long sliding_size = atoi(argv[6]);
	long seq_time, par_time;
	std::vector<ssize_t> collection_par, collection_seq;
	std::cout << "---- Preparing Collection ----" << std::endl;

/*		
	unsigned int a = 4294967291/2;
	for(size_t i = a; i > 0; i--){
		if(isPrime(i)){
			std::cout << i << std::endl;
			break;
		}
	}
	return 0;
*/
/*	
	for(auto i = 0; i < n_tasks; i++){
		collection_seq.push_back(std::numeric_limits<int>::max());
		collection_par.push_back(std::numeric_limits<int>::max());
	}
*/	
/*
	for(size_t i = 0; i < n_tasks/3; i++){
		size_t t = 39;// rand() % 3000;
		collection_seq.push_back(t);
		collection_par.push_back(t);
	}
	for(size_t i = n_tasks/3; i < n_tasks*2/3; i++){
		size_t t = 41; //rand() % 3000;
		collection_seq.push_back(t);
		collection_par.push_back(t);
	}
	for(size_t i = n_tasks*2/3; i < n_tasks; i++){
		size_t t = 40; //rand() % 3000;
		collection_seq.push_back(t);
		collection_par.push_back(t);
	}

*/	
	
	for(size_t i = 0; i < n_tasks/3; i++){
		size_t t = 2147483629;// rand() % 3000;
		collection_seq.push_back(t);
		collection_par.push_back(t);
	}
	for(size_t i = n_tasks/3; i < n_tasks*2/3; i++){
		size_t t = 53687090; //rand() % 3000;
		collection_seq.push_back(t);
		collection_par.push_back(t);
	}
	for(size_t i = n_tasks*2/3; i < n_tasks; i++){
		size_t t = 4294967291; //rand() % 3000;
		collection_seq.push_back(t);
		collection_par.push_back(t);
	}


/*	
	size_t val;// = 4294967291, 536870909, 2147483629;
	for(size_t i = 0; i < n_tasks; i++){
		collection_seq.push_back(val);
		collection_par.push_back(val);
	}
*/
	std::cout << collection_seq.size() << std::endl;

	std::cout << "---- Computing ----" << std::endl;
	par_time = parallel(ts_goal, n_threads, n_max_threads, isPrime, buffer_len, &collection_par, sliding_size);
	std::cout << "Par_TIME: " << par_time << std::endl;

	seq_time = sequential(isPrime, &collection_seq);
	std::cout << "Seq_TIME: " << seq_time << std::endl;

	std::cout << "Scalability " << (float) seq_time/par_time << std::endl;
/*	for(auto i : collection_par)
		std::cout << i << std::endl;
	
	std::cout << "----  ----" << std::endl;
	for(auto i : collection_seq)
		std::cout << i << std::endl;
*/
	std::cout << "Are Equal? " << (collection_par == collection_seq) << std::endl;

	return 0;
}


