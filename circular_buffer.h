#include <cstdlib>
#include <atomic>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono>

#define EOS -1

class Circular_Buffer_Barrier{
	private:
		void** circular_buffer;
		unsigned long p_read=0, p_write=0, size;
		std::mutex* d_mutex;
		std::condition_variable* p_condition;        //producer 
		std::condition_variable* c_condition;        //consumer
	

	public:
		Circular_Buffer_Barrier(unsigned long size){
			this->size = size;
			this->circular_buffer = (void**) malloc(size*sizeof(void*)); //o size_t?
			for(unsigned long i = 0; i < this->size; i++)
				circular_buffer[i] = NULL;
			d_mutex = new std::mutex();
			p_condition = new std::condition_variable();
			c_condition = new std::condition_variable();

		}


		bool push(void* const task){
			if(this->circular_buffer[this->p_write] == NULL){   //if holds, it goes through
				std::atomic_thread_fence(std::memory_order_release);
				circular_buffer[p_write] = task;
				this->p_write = (this->p_write == this->size - 1) ? 0 : this->p_write + 1;
				return true;
			}
			return false;
		}

		bool pop(void  **task){ 
			if(this->circular_buffer[this->p_read] != NULL){   //if holds, it goes through
				*task = this->circular_buffer[this->p_read];
				std::atomic_thread_fence(std::memory_order_acquire);
				this->circular_buffer[this->p_read] = NULL;
				this->p_read = (this->p_read == this->size - 1) ? 0 : this->p_read + 1;
				return true;
			}
			return false;
		}

		void print(){
			for(int i = 0; i < this->p_write-1; i++){
				std::cout << *((int*) this->circular_buffer[i]) << std::endl;
			}
		}

};
