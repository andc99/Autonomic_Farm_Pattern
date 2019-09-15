#include "lock_buffer.h"

class Circular_Buffer : public Lock_Buffer{
	private:
		void** circular_buffer;
		size_t p_read = 0, p_write = 0;
	
	public:

		Circular_Buffer(size_t size);

		~Circular_Buffer();

		bool safe_push(void* const task);

		bool try_safe_push(void* const task); //if false go next queue

		bool safe_pop(void **task);

		bool try_safe_pop(void **task); //if false go next queue

		void safe_resize(size_t new_size);

		size_t safe_get_size();

};
