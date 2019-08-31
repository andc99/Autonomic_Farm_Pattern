#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <iostream>

class Buffer{
	protected:
		size_t size = 0;
		long mean_push_rate = 0, mean_pop_rate = 0; 
		long pushed_elements = 0, popped_elements = 0; 
		std::mutex* d_mutex;
		std::chrono::high_resolution_clock::time_point prev_push_time, prev_pop_time;	
		Buffer(size_t size);
		~Buffer();

		void update_mean_push_rate();

		void update_mean_pop_rate();

	public:
		virtual bool safe_push(void* const task) = 0;

		virtual bool safe_pop(void **task) = 0;

		virtual void safe_resize(size_t new_size) = 0;

		virtual size_t safe_get_size() = 0;

		bool is_bottleneck();

};

