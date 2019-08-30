#include <thread>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <vector>
#include <queue>
#include <unordered_map> 
#include <functional>
#include <set>

#ifdef CB
	#include "./buffers/circular_buffer.h"
	#define BUFFER Circular_Buffer
#elif SQ
	#include "./buffers/safe_queue.h"
	#define BUFFER Safe_Queue
#else
	#include "./buffers/free_circular_buffer.h"
	#define BUFFER Free_Circular_Buffer
#endif

#define EOS ((void*)-1)

class Autonomic_Farm;
class Manager;
class Core;
class Emitter;
class Worker;
class Collector;
/////////////////////////////////////////////////////////////////////////
//
//	ProcessingElement	
//
/////////////////////////////////////////////////////////////////////////

class ProcessingElement{
	protected:
		size_t thread_id;
		size_t context_id = 0;
		std::thread* thread;
		std::mutex *context_id_lock, *stats_lock;
		long processed_elements = 0;
		long mean_service_time = 0;
		long variance_service_time = 0;

		virtual void body() = 0;
		virtual void run() = 0;

		ProcessingElement();

		~ProcessingElement();

		void update_stats(long act_service_time);

		long update_mean_service_time(long act_service_time);

		long update_variance_service_time(long act_service_time, long pred_mean_service_time);
		

	public:
		void join();
		
		size_t get_id();

		size_t get_context();

		void set_context(size_t context_id);

		ssize_t move_to_context(size_t id_context);

		long get_mean_service_time();

		long get_variance_service_time();

};

/////////////////////////////////////////////////////////////////////////
//
//	Emitter
//
/////////////////////////////////////////////////////////////////////////

class Emitter : public ProcessingElement{
	private:
		const unsigned int n_buffers;
		std::function<void(void*)> next_push;
		BUFFER* emitter_buffer; //potrebbe non essere necessario, dovrebbero esserfe concatenabili (?)
		std::vector<ssize_t>* collection;
		
		std::function<void(void*)> rotate_push(std::vector<BUFFER*>* buffers){
			size_t id_queue = 0;
			return [id_queue, buffers](void* task) mutable {
				((*buffers)[id_queue]->BUFFER::safe_push)(task);
				(id_queue < buffers->size()-1) ? id_queue++ : id_queue = 0;	
			};
		}

	public:
		Emitter(std::vector<BUFFER*>* win_bfs, size_t buffer_len, std::vector<ssize_t>* collection);

		void body();

		void run();

		BUFFER* get_in_buffer();	

};


/////////////////////////////////////////////////////////////////////////
//
//	Worker	
//
/////////////////////////////////////////////////////////////////////////

class Worker : public ProcessingElement{
	private:
		std::function<ssize_t(ssize_t)> fun_body;
		BUFFER* win_bf;
		BUFFER* wout_bf;

	public: 

		Worker(std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len);

		void body();

		void run();

		BUFFER* get_in_buffer();

		BUFFER* get_out_buffer();
};


/////////////////////////////////////////////////////////////////////////
//
//	Collector	
//
/////////////////////////////////////////////////////////////////////////

class Collector: public ProcessingElement{
	private:
		const unsigned int n_buffers;
		std::function<void(void**)> next_pop;
		BUFFER* collector_buffer;

		std::function<void(void**)> rotate_pop(std::vector<BUFFER*>* buffers){
			size_t id_queue{0};
			return [id_queue, buffers](void** task) mutable {
				((*buffers)[id_queue]->BUFFER::safe_pop)(task);
				(id_queue < buffers->size()-1) ? id_queue++ : id_queue = 0;	
			};
		}

	public:
		Collector(std::vector<BUFFER*>* wout_bfs, size_t buffer_len);

		void body();

		void run();

		BUFFER* get_out_buffer();
};


/////////////////////////////////////////////////////////////////////////
//
//	Context	
//
/////////////////////////////////////////////////////////////////////////
class Context{
	private:
		const unsigned int id_context;
		std::deque<ProcessingElement*> trace;	
	
	public:
		Context(unsigned id_context);

		unsigned int get_id_context();

		unsigned int get_n_threads();

		std::deque<ProcessingElement*> get_trace();

		void move_in(ProcessingElement* pe);

		ProcessingElement* move_out();

};


/////////////////////////////////////////////////////////////////////////
//
//	Manager	
//
/////////////////////////////////////////////////////////////////////////


class Manager : public ProcessingElement{
	private:
		unsigned int nw;
		const long ts_goal;
		const unsigned int max_nw;
		std::atomic<bool>* stop;
		std::deque<Context*> active_contexts = std::deque<Context*>();
		std::deque<Context*> idle = std::deque<Context*>();
		Emitter* emitter;
		Collector* collector;
		std::deque<ProcessingElement*> pes_queue;

		
		void wake_workers(unsigned int n);

		void idle_workers(unsigned int n);
		
		long get_service_time_farm();

		void transfer_threads_to_core(Context* from, Context* to); 
		
		void redistribute();

		void resize(Context* context, unsigned int size);

	public:
		Manager(long ts_goal, std::atomic<bool>* stop,
				Emitter* emitter,
				Collector* collector,
				std::vector<ProcessingElement*>* workers,
				unsigned int nw, unsigned int max_nw, unsigned int n_contexts);

		void body();

		void run();

}; 

/////////////////////////////////////////////////////////////////////////
//
//	Autonomic Farm	
//
/////////////////////////////////////////////////////////////////////////

class Autonomic_Farm{
	private:
		const long ts_goal; //non dovrebbe servire
		const std::function<ssize_t(ssize_t)> fun_body;
		unsigned int max_nw;
		std::atomic<bool>* stop;
		Manager* manager;
		Emitter* emitter;
		std::vector<Worker*>* workers;
		Collector* collector;

		Worker* add_worker(std::vector<BUFFER*>* win_bfs, std::vector<BUFFER*>* wout_bfs, size_t buffer_len);


	public:

		Autonomic_Farm(long ts_goal, unsigned int nw, unsigned int max_nw, std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len, std::vector<ssize_t>* collection);

		void run();

		void join(); 

		void run_and_wait();

		size_t pop_outputs();
};

