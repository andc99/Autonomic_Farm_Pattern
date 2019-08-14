#include <atomic>
#include <mutex>
#include <condition_variable>

class Free_Circular_Buffer{
	private:
		void** free_circular_buffer;
		size_t p_read = 0, p_write = 0, size = 0;
		std::mutex* d_mutex;

	
	public:

		Free_Circular_Buffer(size_t size);

		~Free_Circular_Buffer();

		bool safe_push(void* const task);

		bool try_safe_push(void* const task); //if false go next queue

		bool safe_pop(void **task);

		void safe_resize(size_t new_size);

		size_t safe_get_size();

};
