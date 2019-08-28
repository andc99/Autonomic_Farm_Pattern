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
		std::vector<Buffer*>* win_cbs; //input queues to workers
		Buffer* emitter_cb; //potrebbe non essere necessario, dovrebbero esserfe concatenabili (?)
		std::vector<ssize_t>* collection;

	public:
		Emitter(std::vector<Buffer*>* win_cbs, size_t buffer_len, std::vector<ssize_t>* collection);

		void body();

		void run();

		Buffer* get_in_queue();

};


/////////////////////////////////////////////////////////////////////////
//
//	Worker	
//
/////////////////////////////////////////////////////////////////////////

class Worker : public ProcessingElement{
	private:
		std::function<ssize_t(ssize_t)> fun_body;
		Buffer* win_cb;
		Buffer* wout_cb;

	public: 

		Worker(std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len);

		void body();

		void run();

		Buffer* get_in_queue();

		Buffer* get_out_queue();
};


/////////////////////////////////////////////////////////////////////////
//
//	Collector	
//
/////////////////////////////////////////////////////////////////////////

class Collector: public ProcessingElement{
	private:
		std::vector<Buffer*>* wout_cbs;
		Buffer* collector_cb;
		std::function<Buffer*()> next_buffer;

	public:
		Collector(std::vector<Buffer*>* wout_cbs, size_t buffer_len);

		void body();

		void run();

		Buffer* get_out_queue();
};


/////////////////////////////////////////////////////////////////////////
//
//	Manager	
//
/////////////////////////////////////////////////////////////////////////
class Context{
	private:
		static std::deque<ProcessingElement*> pes_queue;
		const unsigned int id_context;
		std::deque<ProcessingElement*> trace;
		
		void resize(unsigned int size);
	
	public:
		Context(unsigned id_context);

		unsigned int get_n_threads();

		void move_in(ProcessingElement* pe);

		ProcessingElement* move_out();

		void transfer_threads_to_core(Context* context); //devo poi aggiornare gli active e idle da fuori
		
		static void Redistribute(std::deque<Context*>* active_contexts, unsigned int max_nw, unsigned int nw);

};

class Manager : public ProcessingElement{
	private:
		unsigned int nw;
		const long ts_goal;
		std::atomic<bool>* stop;
		Autonomic_Farm* autonomic_farm;
		const unsigned int max_nw;
		std::deque<Context*> active_contexts = std::deque<Context*>();
		std::deque<Context*> idle = std::deque<Context*>();

		
		void wake_workers(unsigned int n);

		void idle_workers(unsigned int n);

	public:
		Manager(Autonomic_Farm* autonomic_farm, long ts_goal, std::atomic<bool>* stop, ProcessingElement* emitter,
				ProcessingElement* collector,
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
		unsigned int nw; //--------------------- serve sempre atomic? levo
		unsigned int max_nw;
		std::atomic<bool>* stop;
		Manager* manager;
		Emitter* emitter;
		std::vector<Buffer*>* win_cbs;
		std::vector<Worker*>* workers;
		const std::function<ssize_t(ssize_t)> fun_body;
		std::vector<Buffer*>* wout_cbs;
		Collector* collector;

		Worker* add_worker(size_t buffer_len);


	public:

		Autonomic_Farm(long ts_goal, unsigned int nw, unsigned int max_nw, std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len, std::vector<ssize_t>* collection);

		void run_and_wait();

		long get_service_time_farm();
		
		//void push(I task); <-- dipende
		//O pop();
};

