#include <thread>
#include <chrono>
#include <functional>
#include <atomic>
#include "safe_queue.h"
#define EOS -1

//invece di eliminare un worker, basta metterlo 1 su un core dove c'è già un altro core

class ProcessingElement: public std::thread{
	protected:
		std::thread* thread;
			
};

template<class I, class O> class Worker{
	private:
		int thread_id;
		std::thread* thread;		
		std::function<I(O)> body;
		SafeQueue<I>* in_queue;
		SafeQueue<O>* out_queue;
		//std::function<void(int)> body;
	public:
		Worker(std::function<I(O)> body, SafeQueue<I>* in_queue, SafeQueue<O>* out_queue){
			static std::atomic<int> id{0};
			this->thread_id = id++;
			std::cout << "Worker_id " << thread_id << std::endl;
			this->body = body;
			this->in_queue = in_queue;
			this->out_queue = out_queue;
		}
	

		int get_id(){
			return this->thread_id;
		}
	
		int get_context(){
			return sched_getcpu();	
		}
	
		void join(){
			this->thread->join();
		}
	
		void run(){
			this->thread = new std::thread([=] {   //passare puntatori dei task come in FF!!!!
						I task;
						while( (task = this->in_queue->safe_pop()) != EOS){	
							O res = body(task);
							this->out_queue->safe_push(res);
						}
						this->out_queue->safe_push(EOS);
						return;
					}
				); //Se task is -1 retur
			return; //mettere già qua la join?
		}
	
		int move_to_context(int id_context){
			cpu_set_t cpuset;
			CPU_ZERO(&cpuset);
			CPU_SET(id_context, &cpuset);
			int error = pthread_setaffinity_np((this->*thread).native_handle(), sizeof(cpu_set_t), &cpuset);
			if (error != 0)
				std::cout << "Error calling pthread_setaffinity_np: " << error << "\n";
			return error;
		}

};


template<class T> class Emitter{ //fare una sorta di superclasse (gestire intelligentemente i body diversi che condivide move_to_context ecc
	private:
		int nw; //magari dovrebbe essere atomic int o altro. Se prendo la size delle vect_queue non mi serve
		std::thread* thread;
		SafeQueue<T>* in_queue;
		std::vector<SafeQueue<T>*>* out_queues; //ci sarà da sincronizzare il puntatore per la lista di queue
		//funzione emitter con possibilità di specificare la policy per la distribuzione dei task


	public:
		Emitter(SafeQueue<T>* in_queue, std::vector<SafeQueue<T>*>* out_queues){
			this->in_queue = in_queue;
			this->out_queues = out_queues;
			std::cout << "Emitter" << std::endl;
		}

		void run(){
			this->thread = new std::thread([&] {
				T task;
				int id_worker{0};
				while( (task = this->in_queue->safe_pop()) != EOS){
					(*out_queues)[id_worker]->safe_push(task);	
					id_worker = (++id_worker)%this->out_queues->size(); //c'è da sincronizzare la size in quanto è un vect. Lo riassegno per evitare che id_worker diventi un long e dare errori
					//std::cout << "Emitter id_worker queue: " << id_worker << std::endl;
				}
				for(int i = 0; i < out_queues->size(); i++)
					(*out_queues)[i]->safe_push(EOS);
				return;
			});
		}

		int move_to_context(int id_context){
			cpu_set_t cpuset;
			CPU_ZERO(&cpuset);
			CPU_SET(id_context, &cpuset);
			int error = pthread_setaffinity_np((this->*thread).native_handle(), sizeof(cpu_set_t), &cpuset);
			if (error != 0)
				std::cout << "Error calling pthread_setaffinity_np: " << error << "\n";
			return error;
		}

};

template<class T> class Collector{//se lascio la possibilità di definire il body per il collector è come se lasciassi la possibilità di rimuoverlo ehhh
	private:
		std::thread* thread;
		std::vector<SafeQueue<T>*>* in_queues; //ci sarà da sincronizzare il puntatore per la lista di queue
		SafeQueue<T>* out_queue;
		//funzione emitter con possibilità di specificare la policy per la distribuzione dei task
		

	public:
		Collector(std::vector<SafeQueue<T>*>* in_queues, SafeQueue<T>* out_queue){
			this->in_queues = in_queues;
			this->out_queue = out_queue;
			std::cout << "Collector" << std::endl;
		}

		void run(){
			this->thread = new std::thread([&] {
				T task;
				int id_worker{0};
				while( (task = (*in_queues)[id_worker]->safe_pop()) != EOS){
					this->out_queue->safe_push(task);	
					id_worker = (++id_worker)%this->in_queues->size(); //c'è da sincronizzare la size in quanto è un vect. Lo riassegno per evitare che id_worker diventi un long e dare errori
				}
				this->out_queue->safe_push(EOS);
				return;
			});
		}

		void join(){
			this->thread->join();
		}

		int move_to_context(int id_context){
			cpu_set_t cpuset;
			CPU_ZERO(&cpuset);
			CPU_SET(id_context, &cpuset);
			int error = pthread_setaffinity_np((this->*thread).native_handle(), sizeof(cpu_set_t), &cpuset);
			if (error != 0)
				std::cout << "Error calling pthread_setaffinity_np: " << error << "\n";
			return error;
		}
};

template<class I, class O> class Autonomic_Farm{
	
	private:
		int nw, nw_max;
		//potrei toglierli i vector di code, occupano memoria. Però dall'altra evito di fare jump sulla memoria
		std::vector<SafeQueue<I>*> in_queues; //sarà poi nw_max
		std::vector<SafeQueue<O>*> out_queues;	//sarà poi nw_max	
		SafeQueue<I> in_farm_queue;
		SafeQueue<O> out_farm_queue;
		std::vector<Worker<I,O>*> workers;
		std::function<I(O)> body;
		Emitter<I>* emitter;
		Collector<O>* collector;

	public: //la queue dell'emitter deve essere accedibile dall'esterno oppure passo direttamente la collection in input alla farm
		Autonomic_Farm(std::function<I(O)> body, int nw, int nw_max){ //mette nw-nw_max in stato di ready
			this->body = body;
			unsigned num_cpus = std::thread::hardware_concurrency();	
			this->nw = nw;
			this->nw_max = nw_max;
			this->emitter = new Emitter<I>(&in_farm_queue, &in_queues);
			this->collector = new Collector<O>(&out_queues, &out_farm_queue);
			this->in_queues.reserve(nw);
			this->out_queues.reserve(nw);
			SafeQueue<I>* in_queue;
			SafeQueue<O>* out_queue;
			Worker<I,O>* w;
			for(int i = 0; i < nw; i++){ //sarà nw_max
				in_queue = new SafeQueue<I>();
				out_queue = new SafeQueue<O>();
				w = new Worker<I,O>(body, in_queue, out_queue);
				this->in_queues.push_back(in_queue);
				this->out_queues.push_back(out_queue);
				this->workers.push_back(w); 
			}	
		}

		void run(){
			this->emitter->run();
			for(int i = 0; i < this->nw; i++)
				this->workers[i]->run();
			this->collector->run();
		}

		void push(I task){
			this->in_farm_queue.safe_push(task);
		}

		O pop(){
			return this->out_farm_queue.safe_pop();
		}


};
