#include <atomic>
#include <mutex>
#include <condition_variable>
#include <iostream>

class Buffer{
	protected:
		size_t size;
		std::mutex* d_mutex;
		std::condition_variable* p_condition; //producer
		std::condition_variable* c_condition; //consumer;

		Buffer(size_t size);
	
		~Buffer();

	public:

		virtual bool safe_push(void* const task) = 0;

		virtual bool try_safe_push(void* const task) = 0; //if false go next queue

		virtual bool safe_pop(void **task) = 0;

		virtual void safe_resize(size_t new_size) = 0;

		virtual size_t safe_get_size() = 0;

};
