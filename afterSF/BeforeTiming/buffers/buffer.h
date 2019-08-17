#include <atomic>
#include <mutex>
#include <condition_variable>


class Buffer{
	protected:
		std::mutex* d_mutex;
		size_t size = 0;
		
		Buffer(size_t size);
		~Buffer();
		
	public:
		virtual bool safe_push(void* const task) = 0;

		virtual bool safe_pop(void **task) = 0;

		virtual void safe_resize(size_t new_size) = 0;

		virtual size_t safe_get_size() = 0;

};

