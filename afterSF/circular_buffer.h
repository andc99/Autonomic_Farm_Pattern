#include <atomic>
#include <mutex>
#include <condition_variable>
#include <limits>

class Circular_Buffer{
	private:
		void** circular_buffer;
		unsigned int p_read = 0, p_write = 0, size = 0;
		std::mutex* d_mutex;
		std::mutex* v_mutex;
		std::condition_variable* p_condition; //producer
		std::condition_variable* c_condition; //consumer;

		void init();
	
	public:
		Circular_Buffer();

		Circular_Buffer(unsigned int size);

		~Circular_Buffer();

		bool safe_push(void* const task);

		bool try_safe_push(void* const task); //if false go next queue

		bool safe_pop(void **task);

		void safe_resize(unsigned int new_size);

		unsigned int safe_get_size();

};
