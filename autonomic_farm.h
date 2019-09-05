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
		long service_time = 0;	

		virtual void body() = 0;
		virtual void run() = 0;

		ProcessingElement();

		~ProcessingElement();

		void update_ts(long act_service_time);
		

	public:
		void join();
		
		size_t get_id();

		size_t get_context();

		void set_context(size_t context_id);

		int move_to_context(size_t id_context);

		long get_ts();


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

		void body();
		
		std::function<void(void*)> rotate_push(std::vector<BUFFER*>* buffers){
			size_t id_queue = 0;
			return [id_queue, buffers](void* task) mutable {
				((*buffers)[id_queue]->BUFFER::safe_push)(task);
				(id_queue < buffers->size()-1) ? id_queue++ : id_queue = 0;	
			};
		}
		

	public:
		Emitter(std::vector<BUFFER*>* win_bfs, size_t buffer_len, std::vector<ssize_t>* collection);

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

		void body();

	public: 

		Worker(std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len);

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

		void body();

		std::function<void(void**)> rotate_pop(std::vector<BUFFER*>* buffers){
			size_t id_queue{0};
			return [id_queue, buffers](void** task) mutable {
				((*buffers)[id_queue]->BUFFER::safe_pop)(task);
				(id_queue < buffers->size()-1) ? id_queue++ : id_queue = 0;	
			};
		}


	public:
		Collector(std::vector<BUFFER*>* wout_bfs, size_t buffer_len);

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
	
	public:
		Context(size_t context_id);

		~Context();

		size_t get_context_id();

		size_t get_n_threads();

		std::deque<Worker*>* get_trace();

		void move_in(Worker* pe);

		Worker* move_out();

		long get_avg_ts();

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
		std::atomic<bool>* stop;
		const size_t max_nw;
		const long ts_goal;
		long ts_upper_bound, ts_lower_bound; 
		Emitter* emitter;
		Collector* collector;
		std::deque<Context*> active_contexts = std::deque<Context*>();
		std::deque<Context*> idle_contexts = std::deque<Context*>();
		std::deque<Worker*> ws_queue;

		std::queue<size_t>* nw_series; 
		long pos = 0, sliding_size, acc = 0;
	

		void set_workers(size_t nw);

		void wake_worker();

		void idle_worker();
		
		void control_nw_policy(long farm_ts);

		bool detect_bottlenecks();
	
		void redistribute();

		void resize(Context* context, size_t size);

		void update_nw_moving_avg(size_t new_value);

		size_t get_nw_moving_avg();

		long get_service_time_farm();
	
		void body();

	/*	
		void is_application_overlayed();

		void transfer_threads_to_idle_core(Context*& from); 

		void update_contexts_stats();
		
		long get_contexts_avg_ts();

		void set_contexts_avg_ts(long new_value);
		
	*/


	public:
		Manager(long ts_goal, std::atomic<bool>* stop,
				Emitter* emitter,
				Collector* collector,
				std::vector<Worker*>* workers,
				size_t nw, size_t max_nw, size_t n_contexts, long sliding_size);


		~Manager();

		void run();

		void info();	

}; 

/////////////////////////////////////////////////////////////////////////
//
//	Autonomic Farm	
//
/////////////////////////////////////////////////////////////////////////

class Autonomic_Farm{
	private:
		const std::function<ssize_t(ssize_t)> fun_body;
		unsigned int max_nw;
		std::atomic<bool>* stop;
		Manager* manager;
		Emitter* emitter;
		std::vector<Worker*>* workers;
		Collector* collector;

		Worker* add_worker(std::vector<BUFFER*>* win_bfs, std::vector<BUFFER*>* wout_bfs, size_t buffer_len);


	public:

		Autonomic_Farm(long ts_goal, size_t nw, size_t max_nw, std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len, std::vector<ssize_t>* collection, long sliding_size);

		void run();

		void join(); 

		void run_and_wait();

		size_t pop_outputs();
};

