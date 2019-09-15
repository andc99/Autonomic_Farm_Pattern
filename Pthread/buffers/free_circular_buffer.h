#include "free_buffer.h"

class Free_Circular_Buffer : public Free_Buffer{
	private:
		void** free_circular_buffer;
		size_t p_read = 0, p_write = 0;
	
	public:

		Free_Circular_Buffer(size_t size);

		~Free_Circular_Buffer();

		bool safe_push(void* const task);

		bool safe_pop(void **task);

		void safe_resize(size_t new_size);

		size_t safe_get_size();

};
