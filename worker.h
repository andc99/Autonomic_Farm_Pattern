#include <thread>
#include <chrono>
#include <functional>
#include "safe_queue.h"

template<class I, class O> class Worker{
	private:
		int thread_id;
		SafeQueue<I> in_queue;
		SafeQueue<O> out_queue;
		std::thread* thread;		
		std::function<I(O)> body;
		//std::function<void(int)> body;
	public:
		Worker(int thread_id, std::function<I(O)> body){
			this->thread_id = thread_id;
			this->body = body;
		}
	

	int get_id(){
		return this->thread_id;
	}

	int get_context(){
		return sched_getcpu();	
	}

	void push(O task){
		this->in_queue.safe_push(task);
	}
	
	void join(){
		this->thread->join();
	}

	void run(){
		thread = new std::thread([=] {   //passare puntatori dei task come in FF!!!!
					I task = this->in_queue.safe_pop();
					O res = body(task);
					this->out_queue.safe_push(res);
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


template<class T> class Emitter{
	private:
		int nw; //magari dovrebbe essere atomic int o altro. Se prendo la size delle vect_queue non mi serve
		std::vector<SafeQueue<T>> vect_out_queue; //ci sarà da sincronizzare il puntatore per la lista di queue
	public:
		Emitter(std::vector<SafeQueue<T>> vect_out_queue){
			this->vect_out_queue = vect_out_queue;
		}




};

