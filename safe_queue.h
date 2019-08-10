#include <limits>
#include <queue>
#include <iostream>
#include <unistd.h>
#include <mutex>
#include <condition_variable>
#include <chrono>

using namespace std::chrono;

template <class T> class SafeQueue{ 
	private:
		std::mutex* d_mutex;
		std::condition_variable* p_condition;        //producer 
		std::condition_variable* c_condition;        //consumer
		std::queue<T> queue;  //initialize a local queue
		int max_size;
		long overhead_push = 0;
		long overhead_pop = 0;

	public:
		SafeQueue(int my_size) : max_size(my_size) {}; //ERRORE NON INIZIALIZZO LE CONDITION
		SafeQueue(){
			this->max_size = std::numeric_limits<unsigned int>::max();
			d_mutex = new std::mutex();
			p_condition = new std::condition_variable();
			c_condition = new std::condition_variable();
		};


		void safe_push(T item){  //max size		
			high_resolution_clock::time_point start_time = high_resolution_clock::now();
			std::unique_lock<std::mutex> lock(*d_mutex); 
			this->p_condition->wait(lock, [=]{return this->queue.size() < this->max_size;});   //if holds, it goes through
			this->queue.push(item);
			c_condition->notify_one();       //wake a consumer since 1 element has been pushed in
			high_resolution_clock::time_point end_time = high_resolution_clock::now();
			overhead_push += std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
		}

		bool safe_push_try(T item){ 
			if(d_mutex->try_lock()){
				this->queue.push(item);
				c_condition->notify_one();
				return true;
			}
			return false;
		}

		int safe_size(){
			std::lock_guard<std::mutex> lock(*d_mutex);
			return this->queue.size();
		}

		T safe_pop(){
			high_resolution_clock::time_point start_time = high_resolution_clock::now();
			std::unique_lock<std::mutex> lock(*d_mutex); 
			this->c_condition->wait(lock, [=]{return !this->queue.empty();});   //if holds, it goes through
			T popped = this->queue.front();
			this->queue.pop();
			p_condition->notify_one();      //wake a producer since 1 element has been popped from
			high_resolution_clock::time_point end_time = high_resolution_clock::now();
			overhead_pop += std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
			return popped;
		}

		void empty_and_print(){
			while(!this->queue.empty()){
				T popped = this->queue.front();
				this->queue.pop();
				std::cout << popped << std::endl;
			}
		}

		bool isEmpty(){
			std::unique_lock<std::mutex> lock(*d_mutex);
			return this->queue.empty();
		}

		long get_overhead_push(){
			return this->overhead_push;
		}

		
		long get_overhead_pop(){
			return this->overhead_pop;
		}


};
