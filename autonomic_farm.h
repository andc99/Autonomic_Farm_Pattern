#include <thread>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <vector>
#include <functional>

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


/////////////////////////////////////////////////////////////////////////
//
//	ProcessingElement	
//
/////////////////////////////////////////////////////////////////////////

class ProcessingElement{
	protected:
		size_t thread_id;
		size_t context_id;
		std::thread* thread;
		std::mutex *context_id_lock, *stats_lock;
		long processed_elements = 0;
		long mean_service_time = 0;
		long variance_service_time = 0;

		virtual void body() = 0;
		virtual void run() = 0;

		ProcessingElement(size_t context_id);

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
		Emitter(std::vector<Buffer*>* win_cbs, size_t buffer_len, std::vector<ssize_t>* collection, size_t context_id);

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

		Worker(std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len, size_t context_id);

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

	public:
		Collector(std::vector<Buffer*>* wout_cbs, size_t buffer_len, size_t context_id);

		void body();

		void run();

		Buffer* get_out_queue();
};


/////////////////////////////////////////////////////////////////////////
//
//	Autonomic Farm	
//
/////////////////////////////////////////////////////////////////////////

class Autonomic_Farm{
	private:
		std::atomic<size_t> nw;
		size_t max_nw;
		Emitter* emitter;
		std::vector<Buffer*>* win_cbs;
		std::vector<Worker*>* workers;
		std::function<ssize_t(ssize_t)> fun_body;
		std::vector<Buffer*>* wout_cbs;
		Collector* collector;

		Worker* add_worker(size_t buffer_len, size_t nw);

		long get_service_time_farm();


	public:

		Autonomic_Farm(size_t nw, size_t max_nw, std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len, bool sticky, std::vector<ssize_t>* collection);

		void run_and_wait();
		
		//void push(I task); <-- dipende
		//O pop();
};









