#include <thread>
#include <chrono>
#include <functional>
#include <atomic>
#include "safe_queue.h"
#define EOS -1


//nel testo c'è da fornire una collection perciò posso assumere che questa sia gestita dall'emitter
//invece di eliminare un worker, basta metterlo 1 su un core dove c'è già un altro core
class ProcessingElement{
	protected:
		int thread_id;
		std::thread* thread;
		bool sticky;

		ProcessingElement(bool sticky){
			static std::atomic<int> id{0};
			this->thread_id = id++;
			std::cout << "PE_id: " << thread_id << std::endl;	
			this->sticky = sticky;
		}

		virtual void run() = 0;
	public:		
		int get_context(){
			return sched_getcpu();	
		}

		int get_id(){
			return this->thread_id;
		}
	
		void join(){
			this->thread->join();
			return;
		}

		int move_to_context(int id_context){
			cpu_set_t cpuset;
			CPU_ZERO(&cpuset);
			CPU_SET(id_context%std::thread::hardware_concurrency(), &cpuset);
			int error = pthread_setaffinity_np(this->thread->native_handle(), sizeof(cpu_set_t), &cpuset);
			if (error != 0)
				std::cout << "Error calling pthread_setaffinity_np: " << error << "\n";
			return error;
		}
};


//Se gli eleem della farm sono coerenti, non importa fare I e O
template<class I, class O> class Worker: public ProcessingElement{
	private:
		std::function<I(O)> body;
		SafeQueue<I>* in_queue;
		SafeQueue<O>* out_queue;
	public:
		Worker(std::function<I(O)> body, SafeQueue<I>* in_queue, SafeQueue<O>* out_queue, bool sticky) :ProcessingElement(sticky){
			this->body = body;
			this->in_queue = in_queue;
			this->out_queue = out_queue;
		}
	
		void run(){
			//passare puntatori dei task come in FF!!!!
			this->thread = new std::thread([&] {
					if(sticky)
						move_to_context(this->get_id());
					std::cout << "thread_id " << this->get_id() << " context: " << this->get_context() << std::endl;
					I task;
					while( (task = this->in_queue->safe_pop()) != EOS)	
						this->out_queue->safe_push(body(task));	
					this->out_queue->safe_push(EOS);
					return;
				}); 
			return; 
		}
};

//la safe _try lock non sarebbe malissimo
//funzione emitter con possibilità di specificare la policy per la distribuzione dei task
template<class T> class Emitter: public ProcessingElement{ 
	private:
		int nw; //magari dovrebbe essere atomic int o altro. Se prendo la size delle vect_queue non mi serve
		SafeQueue<T>* in_queue;
		std::vector<SafeQueue<T>*>* out_queues; //ci sarà da sincronizzare il puntatore per la lista di queue


	public:
		Emitter(SafeQueue<T>* in_queue, std::vector<SafeQueue<T>*>* out_queues, bool sticky):ProcessingElement(sticky){
			this->in_queue = in_queue;
			this->out_queues = out_queues;
		}
//nguardando tramite htop nella fase di proto si osserva che l'emitter era veramente sotto sfrozo
//nel caso di bdoy semplici, il Tcompoute è minore di Tcommunicate quindi problema per parallel
		void run(){
			this->thread = new std::thread([&] {
					if(sticky)
						move_to_context(this->get_id());
					std::cout << "Emitter " << this->get_id() << " context: " << this->get_context() << std::endl;
					T task;
					int id_queue{0};
					while( (task = this->in_queue->safe_pop()) != EOS){
						(*out_queues)[id_queue]->safe_push(task);	
						//std::cout << "in:" << in_queue->safe_size() << std::endl;
						id_queue = (++id_queue)%this->out_queues->size(); //c'è da sincronizzare la size in quanto è un vect. Lo riassegno per evitare che id_queue diventi un long e dare errori
					}
					std::cout << " EOS---- " << std::endl;
					for(int i = 0; i < out_queues->size(); i++)
						(*out_queues)[i]->safe_push(EOS);
					return;
				});
			return;
		}

};

template<class T> class Collector: public ProcessingElement{
	private:
		int nw;
		std::vector<SafeQueue<T>*>* in_queues;
		//ci sarà da sincronizzare il puntatore per la lista di queue
		SafeQueue<T>* out_queue;
		//policy dinamica che in base al carico del thread assegna in modo diverso?

	public:
		Collector(std::vector<SafeQueue<T>*>* in_queues, SafeQueue<T>* out_queue, bool sticky):ProcessingElement(sticky){
			this->in_queues = in_queues;
			this->out_queue = out_queue;
		}

		void run(){
			this->thread = new std::thread([&] {
					if(sticky)
						move_to_context(this->get_id());
					std::cout << "Collector " << this->get_id() << " context: " << this->get_context() << std::endl;
					T task;
					int id_queue{0};
					while( (task = (*in_queues)[id_queue]->safe_pop()) != EOS){
						this->out_queue->safe_push(task);	
						id_queue = (++id_queue)%this->in_queues->size(); //c'è da sincronizzare la size in quanto è un vect. Lo riassegno per evitare che id_queue diventi un long e dare errori
					//	std::cout << "res: " << task << std::endl;
					}
					this->out_queue->safe_push(EOS);
					return;
				});
			return;
		}

};

template<class I, class O> class Autonomic_Farm{
	private:
		int nw, nw_max;
		//potrei toglierli i vector di code, occupano memoria. Però dall'altra evito di fare jump sulla memoria
		SafeQueue<I> in_farm_queue;
		Emitter<I>* emitter;
		std::vector<SafeQueue<I>*> in_queues; //sarà poi nw_max
		std::vector<Worker<I,O>*> workers;
		std::vector<SafeQueue<O>*> out_queues;	//sarà poi nw_max	
		SafeQueue<O> out_farm_queue;
		Collector<O>* collector;

	public:
		//mette nw-nw_max in stato di ready
		Autonomic_Farm(std::function<I(O)> body, int nw, int nw_max, bool sticky){ 
			unsigned num_cpus = std::thread::hardware_concurrency();	
			this->nw = nw;
			this->nw_max = nw_max;
			this->emitter = new Emitter<I>(&in_farm_queue, &in_queues, sticky);
			this->collector = new Collector<O>(&out_queues, &out_farm_queue, sticky);
			this->in_queues.reserve(nw);
			this->out_queues.reserve(nw);
			SafeQueue<I>* in_queue;
			SafeQueue<O>* out_queue;
			Worker<I,O>* w;
			for(int i = 0; i < nw; i++){ //sarà nw_max
				in_queue = new SafeQueue<I>();
				out_queue = new SafeQueue<O>();
				w = new Worker<I,O>(body, in_queue, out_queue, sticky);
				this->in_queues.push_back(in_queue);
				this->out_queues.push_back(out_queue);
				this->workers.push_back(w); 
			}	
		}

		//ID emitter e collector rispetto a workers_ID
		void run(){
			this->emitter->run();
			this->collector->run();
			for(int i = 0; i < this->nw; i++)
				this->workers[i]->run();
			return;
		}

		void push(I task){
			this->in_farm_queue.safe_push(task);
			return;
		}

		O pop(){
			return this->out_farm_queue.safe_pop();
		}


};
