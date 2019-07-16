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

		bool pop(void **task){
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

int move_to_context(int id_context, std::thread* thread){
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(id_context%std::thread::hardware_concurrency(), &cpuset);
	int error = pthread_setaffinity_np(thread->native_handle(), sizeof(cpu_set_t), &cpuset);
	if (error != 0)
		std::cout << "Error calling pthread_setaffinity_np: " << error << "\n";
	return error;
}


int main(){
	Circular_Buffer_Barrier cbb(50);
	//std::cout << " all " << *((int*)cbb.pop()) << std::endl;
	std::vector<int> vec, res;
	for(int i = 0; i < 500; i++)
		vec.push_back(i);
	vec[vec.size()-1] = EOS;
	std::thread* c = new std::thread([&] {
			move_to_context(0, c);
			std::cout << " Context: " << sched_getcpu() << std::endl;
			std::cout << "push" << std::endl;
			for(int i = 0; i < vec.size(); i++){
				cbb.push(&vec[i]);
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
			}); 
	std::thread* p = new std::thread([&] {
			void* task = NULL;
			move_to_context(4, p);
			std::cout << " Context: " << sched_getcpu() << std::endl;
			int valid = cbb.pop(&task);
			std::cout << "pop" << std::endl;
			while(  *((int*) task) != EOS){
				if(valid)
					res.push_back(*((int*) task));	
				valid = cbb.pop(&task);
				}
			}); 
	p->join();
	for(int i : res)
		std::cout << i << std::endl;
	std::cout << res.size() << std::endl;
	return 0;
}
