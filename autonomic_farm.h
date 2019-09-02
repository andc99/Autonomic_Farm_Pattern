#include <thread>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <vector>
#include <queue>
#include <functional>
#include <fstream>
#include <sstream> 
#include <algorithm> 

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
		std::thread* thread;
		std::mutex *context_id_lock, *stats_lock;
		size_t thread_id, context_id = 0;
		std::chrono::high_resolution_clock::time_point start_time, end_time;
		long sliding_size = 0, moving_avg = 0;
		long pos = 0;
	
		virtual void body() = 0;
		virtual void run() = 0;

		ProcessingElement(long sliding_size);

		~ProcessingElement();

		void update_stats(long act_service_time);
		

	public:
		void join();
		
		size_t get_id();

		size_t get_context();

		void set_context(size_t context_id);

		int move_to_context(size_t id_context);

		long get_moving_avg_ts();


};

/////////////////////////////////////////////////////////////////////////
//
//	Emitter
//
/////////////////////////////////////////////////////////////////////////

class Emitter : public ProcessingElement{
	private:
		const size_t n_buffers;
		std::function<void(void*)> next_push;
		std::vector<ssize_t>* collection;
		BUFFER* emitter_buffer; 
		
		std::function<void(void*)> rotate_push(std::vector<BUFFER*>* buffers){
			size_t id_queue = 0;
			return [id_queue, buffers](void* task) mutable {
				((*buffers)[id_queue]->BUFFER::safe_push)(task);
				(id_queue < buffers->size()-1) ? id_queue++ : id_queue = 0;	
			};
		}

	public:
		Emitter(std::vector<BUFFER*>* win_bfs, size_t buffer_len, std::vector<ssize_t>* collection, long sliding_size);

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

		Worker(std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len, long sliding_size);

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
		const size_t n_buffers;
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
		Collector(std::vector<BUFFER*>* wout_bfs, size_t buffer_leni, long sliding_size);

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
		const size_t context_id;
		std::deque<Worker*>* trace;	
		long avg_ts = 0, sd_ts = 0;
		//deconstructor delete trace
	
	public:
		Context(size_t context_id);

		size_t get_context_id();

		size_t get_n_threads();

		std::deque<Worker*>* get_trace();

		void move_in(Worker* pe);

		Worker* move_out();

		long get_avg_ts();

		void set_avg_ts(long new_avg);

		/*
		long get_sd_ts();

		void set_sd_ts(long new_sd);
*/
};


/////////////////////////////////////////////////////////////////////////
//
//	Manager	
//
/////////////////////////////////////////////////////////////////////////


class Manager : public ProcessingElement{
	private:
		std::ofstream data;
		size_t nw;
		const size_t max_nw;
		const long ts_goal;
		std::atomic<bool>* stop;
		Emitter* emitter;
		Collector* collector;
		std::deque<Worker*> ws_queue;
		std::deque<Context*> active_contexts = std::deque<Context*>();
		std::deque<Context*> idle_contexts = std::deque<Context*>();
		long contexts_avg_ts = 0;
	
		void wake_workers(size_t n);

		void idle_workers(size_t n);
		
		long get_service_time_farm();

		void transfer_threads_to_idle_core(Context*& from); 
		
		void redistribute();

		void resize(Context* context, size_t size);

	public:
		Manager(long ts_goal, std::atomic<bool>* stop,
				Emitter* emitter,
				Collector* collector,
				std::vector<Worker*>* workers,
				size_t nw, size_t max_nw, size_t n_contexts, long sliding_size);

		void body();

		void run();

		void info();
	
		void update_contexts_stats();
		
		long get_contexts_avg_ts();

		void set_contexts_avg_ts(long new_value);

		void is_application_overlayed();

		void detect_bottlenecks();

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

		Worker* add_worker(std::vector<BUFFER*>* win_bfs, std::vector<BUFFER*>* wout_bfs, size_t buffer_len, long sliding_time);


	public:

		Autonomic_Farm(long ts_goal, size_t nw, size_t max_nw, std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len, std::vector<ssize_t>* collection);

		void run();

		void join(); 

		void run_and_wait();

		size_t pop_outputs();
};

