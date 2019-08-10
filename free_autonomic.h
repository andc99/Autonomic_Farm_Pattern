#include <functional>
#include <atomic>
#include <chrono>
#include "circular_buffer.h" //h
#define EOS -1

using namespace std::chrono;

//ogni worker fa la sua queue!
//da settare anche una dimensione??
class ProcessingElement{
	protected:
		std::thread* thread;
		int thread_id;
		ProcessingElement(){
			static std::atomic<int> id{0};
			this->thread_id = id++;
		}
	
		virtual void run() = 0;

	public:
		void join(){
			this->thread->join();
		}
		
		int get_id(){
			return this->thread_id;
		}

		int get_context(){
			return sched_getcpu();	
		}

		int move_to_context(int id_context){
			cpu_set_t cpuset;
			CPU_ZERO(&cpuset);
			CPU_SET(id_context%std::thread::hardware_concurrency(), &cpuset);
			int error = pthread_setaffinity_np(this->thread->native_handle(), sizeof(cpu_set_t), &cpuset);
			if (error != 0)
				std::cout << "Error calling pthread_setaffinity_np: " << error << "\n";

			std::cout << "ID: " << this->get_id() << " Context: " << this->get_context() << std::endl;
			return error;
		}
	
};



//fare disegno con le nome del code
template<class I> class Emitter: public ProcessingElement{
	private:
		std::vector<Circular_Buffer_Barrier*>* win_cbbs;
		Circular_Buffer_Barrier* emitter_cbb;
		std::vector<I>* collection;
	
	public:
		Emitter(std::vector<Circular_Buffer_Barrier*>* win_cbbs, std::vector<I>* collection):ProcessingElement(){
			this->win_cbbs = win_cbbs;
			this->emitter_cbb = new Circular_Buffer_Barrier(50);
			this->collection = collection;
		}

		void body_emitter(){ 
			long queue_id{0};
			move_to_context(this->get_id());
			//while( (task = this->emitter_queue.safe_pop()) != EOS){
			for(auto i = 0; i < (*collection).size(); i++){
				I &task = (*collection)[i];
				(*win_cbbs)[queue_id]->push(&task);
				queue_id++;
				queue_id = queue_id%win_cbbs->size(); //forse qui èèèèè safe_size
			}

			void* EO = (void*) -1;
			for(auto i = 0; i < win_cbbs->size(); i++){
				std::cout << " <------ "  << std::endl;
				(*win_cbbs)[i]->push(EO);
			}
			return;
		}

		void run(){
			this->thread = new std::thread(&Emitter::body_emitter, this);
			//this->thread->detach();
			return;
		}
		
		Circular_Buffer_Barrier* get_in_queue(){
			return this->emitter_cbb;
		}
};


template<class I, class O> class Worker: public ProcessingElement{
	private:
		std::function<I(O)> body; //con puntatore?
		Circular_Buffer_Barrier* win_cbb;
		Circular_Buffer_Barrier* wout_cbb;
		long processing_time = 0;

	public:
		Worker(std::function<I(O)> body):ProcessingElement(){
			this->win_cbb = new Circular_Buffer_Barrier(50);
			this->wout_cbb = new Circular_Buffer_Barrier(50);
			this->body = body;
		}

		void body_worker(){
			void* task = 0;
			int processed = 0;
			move_to_context(this->get_id());
			bool valid = win_cbb->pop(&task);
			while(  *((I*) task) != -1){
					processed++;
					std::cout << this->get_id() << " " << processed << std::endl;
					I &t = (*((I*) task));	
					std::this_thread::sleep_for(std::chrono::microseconds(10));
					high_resolution_clock::time_point start_time = high_resolution_clock::now();
					//std::cout << this->get_id() << " " << processed << std::endl;
					O res = body(t);	
					high_resolution_clock::time_point end_time = high_resolution_clock::now();   
					processing_time += std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
					this->wout_cbb->push(&res);
				valid = win_cbb->pop(&task);
			}
			//std::cout << this->get_id() << " " << *((I*) task) << std::endl;
			void* EO = (void*) -1;
			this->wout_cbb->push(EO);
			return;
		}

		void run(){
			this->thread = new std::thread(&Worker::body_worker, this);
			//this->thread->detach();
			return;
		}

		Circular_Buffer_Barrier* get_in_queue(){
			return this->win_cbb;
		}

		Circular_Buffer_Barrier* get_out_queue(){
			return this->wout_cbb;
		}

		long get_processing_time(){
			return this->processing_time;
		}
};

template<class O> class Collector: public ProcessingElement{
	private:
		std::vector<Circular_Buffer_Barrier*>* wout_cbbs;
		Circular_Buffer_Barrier* collector_cbb;
	
	public:
		Collector(std::vector<Circular_Buffer_Barrier*>* wout_cbbs):ProcessingElement(){
			this->wout_cbbs = wout_cbbs; 
			this->collector_cbb = new Circular_Buffer_Barrier(50);
		}

		void body_collector(){
			void* task = 0;
			long queue_id{0};
			move_to_context(this->get_id());
			int processed = 0;
			bool valid = (*wout_cbbs)[queue_id]->pop(&task);
			while(  *((O*) task) != -1){
				processed++;
			//	std::cout << processed << std::endl;
					O &res = (*((O*) task));	
				valid = (*wout_cbbs)[queue_id]->pop(&task);
				queue_id++;
				queue_id = queue_id%wout_cbbs->size();
			}
			std::cout << "fine" << std::endl;
			void* EO = (void*) -1;
			this->collector_cbb->push(EO);
			return;
		}

		void run(){
			this->thread = new std::thread(&Collector::body_collector, this);
			//this->thread->detach();
			return;
		}

		Circular_Buffer_Barrier* get_collector_queue(){
			return this->collector_cbb;
		}
};

//deconstructor per eliminare la code nei worker

template<class I, class O> class Autonomic_Farm_Silent{
	private:
		int nw;
		Emitter<I>* emitter;
		Collector<O>* collector;
		std::vector<Circular_Buffer_Barrier*>* win_cbbs;
		std::vector<Circular_Buffer_Barrier*>* wout_cbbs;
		std::vector<Worker<I,O>*>* workers;

	public:	
		Autonomic_Farm_Silent(std::function<I(O)> body, int nw, std::vector<I>* collection){
			this->nw = nw;
			this->win_cbbs = new std::vector<Circular_Buffer_Barrier*>();
			this->wout_cbbs = new std::vector<Circular_Buffer_Barrier*>(); 
			this->workers = new std::vector<Worker<I,O>*>(); 
			Worker<I,O>* worker;
			for(int i = 0; i < nw; i++){
				worker = new Worker<I,O>(body);
				this->win_cbbs->push_back(worker->get_in_queue());
				this->wout_cbbs->push_back(worker->get_out_queue());
				(*this->workers).push_back(worker);
			}
			this->emitter = new Emitter<I>(this->win_cbbs, collection);
			this->collector = new Collector<O>(this->wout_cbbs);		
		}

		void run_and_wait(){
			this->emitter->run();
			for(int i = 0; i < nw; i++)
				(*this->workers)[i]->run();
			this->collector->run();
			this->collector->join();


		}

		void push(I task){
			this->emitter->get_in_queue()->safe_push(task);
			return;
		}

		O pop(){
			return this->collector->get_out_queue()->safe_pop();
		}
	
};
