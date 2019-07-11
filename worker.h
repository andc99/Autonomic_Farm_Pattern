#include <thread>
#include <chrono>
#include <functional>
#include <atomic>
#include "safe_queue.h"
#define EOS -1


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
			std::cout << "id " << thread_id << std::endl;
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
	
		void push(O task){
			this->in_queue->safe_push(task);
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
		SafeQueue<T> in_queue;
		std::vector<SafeQueue<T>*>* out_queues; //ci sarà da sincronizzare il puntatore per la lista di queue
		//funzione emitter con possibilità di specificare la policy per la distribuzione dei task
		void body(){
			T task;
			int id_worker{0};
			while( (task = this->in_queue.safe_pop()) != EOS){
				this->out_queues[id_worker].safe_push(task);	
				id_worker = (++id_worker)%out_queues.size(); //c'è da sincronizzare la size in quanto è un vect. Lo riassegno per evitare che id_worker diventi un long e dare errori
			}
			return;
		}

	public:
		Emitter(std::vector<SafeQueue<T>*>* out_queues){
			this->out_queues = out_queues;
		}

		void run(){
			this->thread = new std::thread(body);
			this->thread->join(); //oppure fuori	
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
		//potrei toglierli i vector di code, occupano memoria. Però dall'altra evito di fare jump sulla memoria
		std::vector<SafeQueue<I>*> in_queues; //sarà poi nw_max
		std::vector<SafeQueue<O>*> out_queues;	//sarà poi nw_max	
		std::vector<Worker<I,O>*> workers;
		std::function<I(O)> body;

	public: //la queue dell'emitter deve essere accedibile dall'esterno oppure passo direttamente la collection in input alla farm
		Autonomic_Farm(std::function<I(O)> body, int nw, int nw_max){ //mette nw-nw_max in stato di ready
			this->body = body;
			in_queues.reserve(nw);
			out_queues.reserve(nw);
			SafeQueue<I>* in_queue;
			SafeQueue<O>* out_queue;
			Worker<I,O>* w;
			for(int i = 0; i < nw; i++){ //sarà nw_max
				std::cout << "entro " << i << " nw " << nw << std::endl;
				in_queue = new SafeQueue<I>();
				out_queue = new SafeQueue<O>();
				w = new Worker<I,O>(body, in_queue, out_queue);
				in_queue->safe_push(3*i);
				in_queues.push_back(in_queue);
				out_queues.push_back(out_queue);
				workers.push_back(w); 
				Emitter<I> emitter(&in_queues);
			}	
		}


};
