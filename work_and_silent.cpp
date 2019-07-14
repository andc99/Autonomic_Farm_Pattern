#include <functional>
#include <atomic>
#include "safe_queue.h"
#define EOS -1

//ogni worker fa la sua queue!
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
	
};


template<class I, class O> class Worker: public ProcessingElement{
	private:
		std::function<I(O)> body; //con puntatore?
		SafeQueue<I>* win_queue;	
		SafeQueue<O>* wout_queue;	

	public:
		Worker(std::function<I(O)> body, SafeQueue<I>* win_queue, SafeQueue<O>* wout_queue):ProcessingElement(){
			this->body = body;
			this->win_queue = win_queue;
			this->wout_queue = wout_queue;
		}

		void body_worker(){
			I task;
			int processed = 0;
			while( (task = this->win_queue->safe_pop()) != EOS){
				this->wout_queue->safe_push(body(task));
			}
			std::cout << this->get_id() << std::endl;
			this->wout_queue->safe_push(EOS);
			return;
		}

		void run(){
			this->thread = new std::thread(&Worker::body_worker, this);
			//this->thread->detach();
			return;
		}
};

//fare disegno con le nome del code
template<class I> class Emitter: public ProcessingElement{
	private:
		std::vector<SafeQueue<I>*>* win_queues;
		SafeQueue<I>* emitter_queue;
	
	public:
		Emitter(std::vector<SafeQueue<I>*>* win_queues, SafeQueue<I>* emitter_queue):ProcessingElement(){
			this->win_queues = win_queues;
			this->emitter_queue = emitter_queue;
		}

		void body_emitter(){
			I task;
			long queue_id{0};
			while( (task = this->emitter_queue->safe_pop()) != EOS){
				(*win_queues)[queue_id]->safe_push(task);
				queue_id++;
				queue_id = queue_id%win_queues->size(); //forse qui èèèèè safe_size
			}
			for(auto i = 0; i < win_queues->size(); i++)
				(*win_queues)[i]->safe_push(EOS);
			return;
		}

		void run(){
			this->thread = new std::thread(&Emitter::body_emitter, this);
			//this->thread->detach();
			return;
		}
};


template<class O> class Collector: public ProcessingElement{
	private:
		std::vector<SafeQueue<O>*>* wout_queues;
		SafeQueue<O>* collector_queue;
	
	public:
		Collector(std::vector<SafeQueue<O>*>* wout_queues, SafeQueue<O>* collector_queue):ProcessingElement(){
			this->wout_queues = wout_queues; 
			this->collector_queue = collector_queue;
		}

		void body_collector(){
			O task;
			long queue_id{0};
			while( (task = (*wout_queues)[queue_id]->safe_pop()) != EOS){
				this->collector_queue->safe_push(task);
				queue_id++;
				queue_id = queue_id%wout_queues->size();
			}
			std::cout << "fine" << std::endl;
			this->collector_queue->safe_push(EOS);
			return;
		}

		void run(){
			this->thread = new std::thread(&Collector::body_collector, this);
			//this->thread->detach();
			return;
		}
};

template<class I, class O> class Autonomic_Farm_Silent{
	private:
		int nw;
		Emitter<I>* emitter;
		Collector<O>* collector;
		std::vector<SafeQueue<I>*>* win_queues;
		std::vector<SafeQueue<I>*>* wout_queues;
		SafeQueue<I>* emitter_queue;
		SafeQueue<O>* collector_queue;
		std::vector<Worker<I,O>*> workers;

	public:	
		Autonomic_Farm_Silent(std::function<I(O)> body, int nw){
			this->nw = nw;
			this->emitter_queue = new SafeQueue<I>();
			this->collector_queue = new SafeQueue<O>();	
			this->win_queues = new std::vector<SafeQueue<I>*>();
			this->wout_queues = new std::vector<SafeQueue<O>*>(); 
			Worker<I,O>* worker;
			SafeQueue<I>* win_queue;
			SafeQueue<O>* wout_queue;
			for(int i = 0; i < nw; i++){
				win_queue = new SafeQueue<I>();
				wout_queue = new SafeQueue<O>();
				worker = new Worker<I,O>(body, win_queue, wout_queue);
				this->win_queues->push_back(win_queue);
				this->wout_queues->push_back(wout_queue);
				this->workers.push_back(worker);
			}
			this->emitter = new Emitter<I>(this->win_queues, this->emitter_queue);
			this->collector = new Collector<O>(this->wout_queues, this->collector_queue);		
		}

		void run_and_wait(){
			std::thread t0([&] {

			this->emitter->run();
			for(int i = 0; i < nw; i++)
				this->workers[i]->run();
			this->collector->run();
			this->collector->join();
			});
			t0.join();
		}

		void push(I task){
			this->emitter_queue->safe_push(task);
			return;
		}

		O pop(){
			return this->collector_queue->safe_pop();
		}
	
};
