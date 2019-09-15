#include "lock_buffer.h"
#include <queue>

class Safe_Queue : public Lock_Buffer{
	private:
		std::queue<void*>* queue;

	public:
		Safe_Queue(size_t size);

		~Safe_Queue();

		bool safe_push(void* const task);

		bool try_safe_push(void* const task); //if false go next queue

		bool safe_pop(void **task);

		bool try_safe_pop(void **task); //if false go next queue

		void safe_resize(size_t new_size);

		size_t safe_get_size();

};
