#include "buffer.h"

class Lock_Buffer : public Buffer{
	protected:
		std::condition_variable* p_condition; //producer
		std::condition_variable* c_condition; //consumer;

		Lock_Buffer(size_t size);
		~Lock_Buffer();

		virtual bool try_safe_push(void* const task) = 0;

		virtual bool try_safe_pop(void **task) = 0;
};
