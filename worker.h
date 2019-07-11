#include <thread>
#include <chrono>
#include <functional>
#include "safe_queue.h"

template<class T> class Worker{
	private:
		int thread_id;
		SafeQueue<T> queue;
		std::thread* thread;		
		std::function<void(T)> body;
		//std::function<void(int)> body;
	public:
		Worker(int thread_id, std::function<void(T)> body){
			this->thread_id = thread_id;
			this->body = body;
		}
	
	void run(){
		thread = new std::thread([=] {T task = this->queue.safe_pop(); body(task);}); //Se task is -1 retur
		return; //mettere già qua la join?
	}

	int get_id(){
		return this->thread_id;
	}

	void push(T task){
		this->queue.safe_push(task);
	}
	
	void join(){
		this->thread->join();
	}

	int move_to_core(int id_core){
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		CPU_SET(id_core, &cpuset);
		int error = pthread_setaffinity_np((this->*thread).native_handle(), sizeof(cpu_set_t), &cpuset);
		if (error != 0)
			std::cerr << "Error calling pthread_setaffinity_np: " << error << "\n";
		return error;
	}

};
