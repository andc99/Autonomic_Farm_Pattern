#include <functional>
#include <atomic>
#include "safe_queue.h"
#define EOS -1

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


template<class I, class O> class Worker: public ProcessingElement{
	private:
		std::function<I(O)> body; //con puntatore?
		SafeQueue<I> win_queue;	
		SafeQueue<O> wout_queue;
		long processing_time = 0;

	public:
		Worker(std::function<I(O)> body):ProcessingElement(){
			this->body = body;
		}

		void body_worker(){
			I task;
			int processed = 0;
			move_to_context(this->get_id());
			while( (task = this->win_queue.safe_pop()) != EOS){
//				std::cout << "ID: " << this->get_id() << " size: " << this->win_queue.safe_size() << std::endl;

				high_resolution_clock::time_point start_time = high_resolution_clock::now();
				O res = body(task);	
				high_resolution_clock::time_point end_time = high_resolution_clock::now();   
				processing_time += std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
				this->wout_queue.safe_push(body(task));
			}
			this->wout_queue.safe_push(EOS);
			return;
		}

		void run(){
			this->thread = new std::thread(&Worker::body_worker, this);
			//this->thread->detach();
			return;
		}

		SafeQueue<I>* get_in_queue(){
			return &this->win_queue;
		}

		SafeQueue<O>* get_out_queue(){
			return &this->wout_queue;
		}

		long get_processing_time(){
			return this->processing_time;
		}
};

//fare disegno con le nome del code
template<class I> class Emitter: public ProcessingElement{
	private:
		std::vector<SafeQueue<I>*>* win_queues;
		SafeQueue<I> emitter_queue;
		std::vector<I>* collection;
	
	public:
		Emitter(std::vector<SafeQueue<I>*>* win_queues, std::vector<I>* collection):ProcessingElement(){
			this->win_queues = win_queues;
			this->collection = collection;
		}

		void body_emitter(){
			I task;
			long queue_id{0};
			move_to_context(this->get_id());
			//while( (task = this->emitter_queue.safe_pop()) != EOS){
			for(I task : *collection){
				(*win_queues)[queue_id]->safe_push(task);
				queue_id++;
				queue_id = queue_id%win_queues->size(); //forse qui èèèèè safe_size
			}

			for(auto i = 0; i < win_queues->size(); i++){
				(*win_queues)[i]->safe_push(EOS);
			}
			return;
		}

		void run(){
			this->thread = new std::thread(&Emitter::body_emitter, this);
			//this->thread->detach();
			return;
		}
		
		SafeQueue<I>* get_in_queue(){
			return &this->emitter_queue;
		}
};


template<class O> class Collector: public ProcessingElement{
	private:
		std::vector<SafeQueue<O>*>* wout_queues;
		SafeQueue<O> collector_queue;
	
	public:
		Collector(std::vector<SafeQueue<O>*>* wout_queues):ProcessingElement(){
			this->wout_queues = wout_queues; 
		}

		void body_collector(){
			O task;
			long queue_id{0};
			move_to_context(this->get_id());
			while( (task = (*wout_queues)[queue_id]->safe_pop()) != EOS){
				//this->collector_queue.safe_push(task);
				queue_id++;
				queue_id = queue_id%wout_queues->size();
			}
			this->collector_queue.safe_push(EOS);
			return;
		}

		void run(){
			this->thread = new std::thread(&Collector::body_collector, this);
			//this->thread->detach();
			return;
		}

		SafeQueue<O>* get_out_queue(){
			return &this->collector_queue;
		}
};

//deconstructor per eliminare la code nei worker

template<class I, class O> class Autonomic_Farm_Silent{
	private:
		int nw;
		Emitter<I>* emitter;
		Collector<O>* collector;
		std::vector<SafeQueue<I>*>* win_queues;
		std::vector<SafeQueue<I>*>* wout_queues;
		std::vector<Worker<I,O>*>* workers;

	public:	
		Autonomic_Farm_Silent(std::function<I(O)> body, int nw, std::vector<I>* collection){
			this->nw = nw;
			this->win_queues = new std::vector<SafeQueue<I>*>();
			this->wout_queues = new std::vector<SafeQueue<O>*>(); 
			this->workers = new std::vector<Worker<I,O>*>(); 
			Worker<I,O>* worker;
			for(int i = 0; i < nw; i++){
				worker = new Worker<I,O>(body);
				this->win_queues->push_back(worker->get_in_queue());
				this->wout_queues->push_back(worker->get_out_queue());
				(*this->workers).push_back(worker);
			}
			this->emitter = new Emitter<I>(this->win_queues, collection);
			this->collector = new Collector<O>(this->wout_queues);		
		}

		void run_and_wait(){
			this->emitter->run();
			for(int i = 0; i < nw; i++)
				(*this->workers)[i]->run();
			this->collector->run();
			this->collector->join();


			long overhead_push = 0, overhead_pop = 0, processing_time = 0;
			overhead_push+=this->emitter->get_in_queue()->get_overhead_push();
			overhead_pop+=this->emitter->get_in_queue()->get_overhead_pop();
			overhead_push+=this->collector->get_out_queue()->get_overhead_push();
			overhead_pop+=this->collector->get_out_queue()->get_overhead_pop();
			for(int i = 0; i < nw; i++){
				overhead_push+=(*this->workers)[i]->get_in_queue()->get_overhead_push();
				overhead_push+=(*this->workers)[i]->get_out_queue()->get_overhead_push();
				overhead_pop+=(*this->workers)[i]->get_in_queue()->get_overhead_pop();
				overhead_pop+=(*this->workers)[i]->get_out_queue()->get_overhead_pop();
			}
				processing_time=(*this->workers)[0]->get_processing_time();
			std::cout << "proc: " << processing_time << std::endl;
				processing_time=(*this->workers)[1]->get_processing_time();
			std::cout << "proc: " << processing_time << std::endl;
				processing_time=(*this->workers)[2]->get_processing_time();
			std::cout << "proc: " << processing_time << std::endl;
			std::cout << "push: " << overhead_push << std::endl;
			std::cout << "popp: " << overhead_pop << std::endl;
			std::cout << "proc: " << processing_time << std::endl;
		}

		void push(I task){
			this->emitter->get_in_queue()->safe_push(task);
			return;
		}

		O pop(){
			return this->collector->get_out_queue()->safe_pop();
		}
	
};
