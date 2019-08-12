#include <thread>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <vector>
#include <functional>

#include "circular_buffer.h"

#define EOS (void *)-1

//gestire gli IFNOTDEF, ad esempio IFNOTDEF circular_buffer, define!

/////////////////////////////////////////////////////////////////////////
//
//	ProcessingElement	
//
/////////////////////////////////////////////////////////////////////////

class ProcessingElement{
	protected:
		bool sticky;
		unsigned int thread_id;
		std::thread* thread;
		virtual void body() = 0;
		virtual void run() = 0;

		ProcessingElement(bool sticky){
			this->sticky = sticky;
			static std::atomic<unsigned int> id{0};
			this->thread_id = id++; // (std::thread::hardware_concurrency);
			std::cout << "ID: " << this->thread_id << std::endl;	
		}

		~ProcessingElement(){ //bypassabile per via dela join
			delete thread;
			return;
		}

	public:
		void join(){
			this->thread->join();
			return;
		}

		unsigned int get_id(){ //calibrato rispetto al tipo di harware_concurrency che è unsigned int	
			return this->thread_id;
		}

		unsigned int get_context(){
			return sched_getcpu();
		}

		unsigned int move_to_context(unsigned int id_context){
			cpu_set_t cpuset;
			CPU_ZERO(&cpuset);
			CPU_SET(id_context%std::thread::hardware_concurrency(), &cpuset); //module %
			int error = pthread_setaffinity_np(this->thread->native_handle(), sizeof(cpu_set_t), &cpuset);
			if (error != 0)
				std::cout << "Error calling pthread_setaffinity_np: " << error << "\n";
			std::cout << "ID: " << this->get_id() << " Context: " << this->get_context() << std::endl;
			return error;
		}
};

/////////////////////////////////////////////////////////////////////////
//
//	Emitter
//
/////////////////////////////////////////////////////////////////////////

template<class I>
class Emitter : public ProcessingElement{
	private:
		std::vector<Circular_Buffer*>* win_cbs; //input queues to workers
		Circular_Buffer* emitter_cb; //potrebbe non essere necessario, dovrebbero esserfe concatenabili (?)
		std::vector<I>* collection;

	public:
		Emitter(std::vector<Circular_Buffer*>* win_cbs, std::vector<I>* collection, bool sticky) : ProcessingElement(sticky){
			this->win_cbs = win_cbs;
			this->emitter_cb = new Circular_Buffer(5); // da cambiare!
			this->collection = collection;
		}

		void body(){
			unsigned int id_queue{0};
			if(this->sticky){move_to_context(this->get_id());}
			for(auto i = 0; i < (*collection).size(); i++){
				I &task = (*collection)[i];
				std::cout << "emi " << task << std::endl;
				(*win_cbs)[id_queue]->safe_push(&task); //safe_try!!!!
				id_queue = (++id_queue)%win_cbs->size();
			}
			for(auto i = 0; i < win_cbs->size(); i++)
				(*win_cbs)[i]->safe_push(EOS); // qui non ci può andare la safe_try
			return;
		}

		void run(){
			this->thread = new std::thread(&Emitter<I>::body, this);
			return;
		}

		Circular_Buffer* get_in_queue(){
			return this->emitter_cb;
		}
};


/////////////////////////////////////////////////////////////////////////
//
//	Worker	
//
/////////////////////////////////////////////////////////////////////////

template<class I, class O>
class Worker : public ProcessingElement{
	private:
		std::function<I(O)> fun_body;
		Circular_Buffer* win_cb;
		Circular_Buffer* wout_cb;

	public: 

		Worker(std::function<I(O)> fun_body, bool sticky):ProcessingElement(sticky){
			this->fun_body = fun_body;
			this->win_cb = new Circular_Buffer(5); /////////yyp
			this->wout_cb = new Circular_Buffer(5); /////////yyp
		};

		void body(){
			void* task = 0;
			if(this->sticky){move_to_context(this->get_id());}
			win_cb->safe_pop(&task);
			while( (I*) task != EOS){ //NON SICURO FUNZIONI
				I &t = (*((I*) task));
				std::cout << "w: " << &t << std::endl;
				O res = fun_body(t);
				std::cout << "w1: " << &res << std::endl;
				this->wout_cb->safe_push(&res); //safe_try
				this->win_cb->safe_pop(&task);	
			};
			this->wout_cb->safe_push(EOS);
			return;
		}

		void run(){
			this->thread = new std::thread(&Worker<I,O>::body, this);
			return;
		}


		Circular_Buffer* get_in_queue(){
			return this->win_cb;
		}

		Circular_Buffer* get_out_queue(){
			return this->wout_cb;
		}

};


/////////////////////////////////////////////////////////////////////////
//
//	Collector	
//
/////////////////////////////////////////////////////////////////////////

template<class O>
class Collector: public ProcessingElement{
	private:
		std::vector<Circular_Buffer*>* wout_cbs;
		Circular_Buffer* collector_cb;

	public:
		Collector(std::vector<Circular_Buffer*>* wout_cbs, bool sticky):ProcessingElement(sticky){
			this->wout_cbs = wout_cbs;
			this->collector_cb = new Circular_Buffer(5); ////////yyp
		}

		void body(){
			void* task = 0;
			unsigned int eos_counter = 0;
			if(this->sticky){move_to_context(this->get_id());}
			unsigned int id_queue{0};
			(*wout_cbs)[id_queue]->safe_pop(&task);
			while( (O*) task != EOS){ //NON SICURO FUNZIONI --> contare EOS!!
				O &t = (*((O*) task));
				std::cout << "c: " << &t << std::endl;
				this->collector_cb->safe_push(&t); //safe_try
				id_queue = (++id_queue)%wout_cbs->size();
				(*wout_cbs)[id_queue]->safe_pop(&task);
			};
			this->collector_cb->safe_push(EOS);
			return;
		}

		void run(){
			this->thread = new std::thread(&Collector<O>::body, this);
			return;
		}

		Circular_Buffer* get_out_queue(){
			return this->collector_cb;
		}
};


/////////////////////////////////////////////////////////////////////////
//
//	Autonomic Farm	
//
/////////////////////////////////////////////////////////////////////////

template<class I, class O>
class Autonomic_Farm{
	private:
		unsigned int nw;
		bool sticky;
		Emitter<I>* emitter;
		std::vector<Circular_Buffer*>* win_cbs;
		std::vector<Worker<I,O>*>* workers;
		std::function<I(O)> fun_body;
		std::vector<Circular_Buffer*>* wout_cbs;
		Collector<O>* collector;

		void add_worker(){
			Worker<I,O>* worker = new Worker<I,O>(this->fun_body, this->sticky);
			this->win_cbs->push_back(worker->get_in_queue());
			this->wout_cbs->push_back(worker->get_out_queue());
			(*this->workers).push_back(worker);
			return;
		}

	public:

		Autonomic_Farm(unsigned int nw,  std::function<I(O)> fun_body, bool sticky, std::vector<I>* collection){ //max nw??
			std::cout << "Machine Hardware Concurrency " << std::thread::hardware_concurrency() << std::endl;
			this->nw = nw;
			this->sticky = sticky;
			this->fun_body = fun_body;
			this->win_cbs = new std::vector<Circular_Buffer*>();
			this->workers = new std::vector<Worker<I,O>*>();
			this->wout_cbs = new std::vector<Circular_Buffer*>();
			for(auto i = 0; i < nw; i++)
				this->add_worker();
			this->emitter = new Emitter<I>(this->win_cbs, collection, this->sticky);
			this->collector = new Collector<O>(this->wout_cbs, this->sticky);
		}
	
		void run_and_wait(){
			this->emitter->run();
			for(auto i = 0; i < nw; i++)
				(*this->workers)[i]->run();
			this->collector->run();
			this->collector->join();
			void* task = 0;
			unsigned int id_queue{0};
			this->collector->get_out_queue()->safe_pop(&task);
			while( (O*) task != EOS){ //NON SICURO FUNZIONI --> contare EOS!!
				O &t = (*((O*) task));
				std::cout << "task: " << t << std::endl;
				this->collector->get_out_queue()->safe_pop(&task);
			};
			std::cout << "fine" <<std::endl;

		}
		//void push(I task); <-- dipende
		//O pop();
};









