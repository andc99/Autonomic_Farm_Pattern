#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>

class Safe_Queue{
	private:
		std::queue<void*>* queue;
		size_t size;
		std::mutex* d_mutex;
		std::condition_variable* p_condition; //producer
		std::condition_variable* c_condition; //consumer;	

	public:
		Safe_Queue(size_t size);

		~Safe_Queue();

		//non andrebbero virtual? altrimenti mi farebbe compilare anche senza queste
		bool safe_push(void* const task);

		bool try_safe_push(void* const task); //if false go next queue

		bool safe_pop(void **task);

		void safe_resize(size_t new_size);

		size_t safe_get_size();

};
