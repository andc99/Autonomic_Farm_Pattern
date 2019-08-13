#include <thread>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <vector>
#include <functional>

#include "circular_buffer.h"

#define EOS ((void*)-1)

//gestire gli IFNOTDEF, ad esempio IFNOTDEF circular_buffer, define!

/////////////////////////////////////////////////////////////////////////
//
//	ProcessingElement	
//
/////////////////////////////////////////////////////////////////////////

class ProcessingElement{
	protected:
		bool sticky;
		size_t thread_id;
		std::thread* thread;
		virtual void body() = 0;
		virtual void run() = 0;

		ProcessingElement(bool sticky){
			this->sticky = sticky;
			static std::atomic<size_t> id{0};
			this->thread_id = id++; // (std::thread::hardware_concurrency);
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

		size_t get_id(){ //calibrato rispetto al tipo di harware_concurrency che è unsigned int	
			return this->thread_id;
		}

		size_t get_context(){
			return sched_getcpu();
		}

		size_t move_to_context(size_t id_context){
			cpu_set_t cpuset;
			CPU_ZERO(&cpuset);
			CPU_SET(id_context%std::thread::hardware_concurrency(), &cpuset); //module %
			size_t error = pthread_setaffinity_np(this->thread->native_handle(), sizeof(cpu_set_t), &cpuset);
			if (error != 0)
				std::cout << "Error calling pthread_setaffinity_np: " << error << "\n";
			return error;
		}
};

/////////////////////////////////////////////////////////////////////////
//
//	Emitter
//
/////////////////////////////////////////////////////////////////////////

class Emitter : public ProcessingElement{
	private:
		std::vector<Circular_Buffer*>* win_cbs; //input queues to workers
		Circular_Buffer* emitter_cb; //potrebbe non essere necessario, dovrebbero esserfe concatenabili (?)
		std::vector<size_t>* collection;

	public:
		Emitter(std::vector<Circular_Buffer*>* win_cbs, size_t buffer_len, bool sticky, std::vector<size_t>* collection) : ProcessingElement(sticky){
			this->win_cbs = win_cbs;
			this->emitter_cb = new Circular_Buffer(buffer_len); 
			this->collection = collection;
		}

		void body(){
			size_t id_queue{0};
			if(this->sticky){move_to_context(this->get_id());}
			for(size_t i = 0; i < (*collection).size(); i++){
				size_t &task = (*collection)[i];
				(*win_cbs)[id_queue]->safe_push(&task); //safe_try!!!!
				id_queue = (++id_queue)%win_cbs->size();
			}
			for(auto i = 0; i < win_cbs->size(); i++)
				(*win_cbs)[i]->safe_push(EOS); // qui non ci può andare la safe_try
			return;
		}

		void run(){
			this->thread = new std::thread(&Emitter::body, this);
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

class Worker : public ProcessingElement{
	private:
		std::function<size_t(size_t)> fun_body;
		Circular_Buffer* win_cb;
		Circular_Buffer* wout_cb;

	public: 

		Worker(std::function<size_t(size_t)> fun_body, size_t buffer_len, bool sticky):ProcessingElement(sticky){
			this->fun_body = fun_body;
			this->win_cb = new Circular_Buffer(buffer_len); 
			this->wout_cb = new Circular_Buffer(buffer_len); 
		};

		void body(){
			void* task = 0;
			if(this->sticky){move_to_context(this->get_id());}
			win_cb->safe_pop(&task);
			while( task != EOS){ 
				size_t &t = (*((size_t*) task));
				t = (fun_body(t));
				this->wout_cb->safe_push(task); //safe_try
				this->win_cb->safe_pop(&task);	
			};
			this->wout_cb->safe_push(EOS);
			return;
		}

		void run(){
			this->thread = new std::thread(&Worker::body, this);
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

class Collector: public ProcessingElement{
	private:
		std::vector<Circular_Buffer*>* wout_cbs;
		Circular_Buffer* collector_cb;

	public:
		Collector(std::vector<Circular_Buffer*>* wout_cbs, size_t buffer_len, bool sticky):ProcessingElement(sticky){
			this->wout_cbs = wout_cbs;
			this->collector_cb = new Circular_Buffer(buffer_len); ////////yyp
		}

		void body(){
			void* task = 0;
			size_t id_queue{0};
			if(this->sticky){move_to_context(this->get_id());}
			(*wout_cbs)[id_queue]->safe_pop(&task);
			size_t eos_counter = (task == EOS) ? 1 : 0;
			while(eos_counter < (*wout_cbs).size()){ 
				size_t &t = (*((size_t*) task));
				//this->collector_cb->safe_push(&t); //safe_try
				id_queue = (++id_queue)%wout_cbs->size();
				(*wout_cbs)[id_queue]->safe_pop(&task);
				eos_counter += (task == EOS) ? 1 : 0;
			};
			this->collector_cb->safe_push(EOS);
			return;
		}

		void run(){
			this->thread = new std::thread(&Collector::body, this);
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

class Autonomic_Farm{
	private:
		size_t nw;
		bool sticky;
		Emitter* emitter;
		std::vector<Circular_Buffer*>* win_cbs;
		std::vector<Worker*>* workers;
		std::function<size_t(size_t)> fun_body;
		std::vector<Circular_Buffer*>* wout_cbs;
		Collector* collector;

		Worker* add_worker(size_t buffer_len){ //c'è da runnarlo poi ehhhh
			Worker* worker = new Worker(this->fun_body, buffer_len, this->sticky);
			this->win_cbs->push_back(worker->get_in_queue());
			this->wout_cbs->push_back(worker->get_out_queue());
			(*this->workers).push_back(worker);
			return worker;
		}

	public:

		Autonomic_Farm(size_t nw, std::function<size_t(size_t)> fun_body, size_t buffer_len, bool sticky, std::vector<size_t>* collection){ //max nw??
			//std::cout << "Machine Hardware Concurrency " << std::thread::hardware_concurrency() << std::endl;
			this->nw = nw;
			this->sticky = sticky;
			this->fun_body = fun_body;
			this->win_cbs = new std::vector<Circular_Buffer*>();
			this->workers = new std::vector<Worker*>();
			this->wout_cbs = new std::vector<Circular_Buffer*>();
			this->emitter = new Emitter(this->win_cbs, buffer_len, this->sticky, collection);
			this->collector = new Collector(this->wout_cbs, buffer_len, this->sticky);
			for(size_t i = 0; i < nw; i++)
				this->add_worker(buffer_len);
		}
	
		void run_and_wait(){
			this->emitter->run();
			for(size_t i = 0; i < nw; i++)
				(*this->workers)[i]->run();	
			this->collector->run();
			(*this->workers)[nw-1]->join();
			this->collector->join();
		}

		//void push(I task); <-- dipende
		//O pop();
};









