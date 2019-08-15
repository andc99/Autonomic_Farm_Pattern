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
		bool sticky;
		size_t thread_id;
		std::thread* thread;
		virtual void body() = 0;
		virtual void run() = 0;

		ProcessingElement(bool sticky);

		~ProcessingElement();

	public:
		void join();

		size_t get_id();

		size_t get_context();

		size_t move_to_context(size_t id_context);
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
		std::vector<size_t>* collection;

	public:
		Emitter(std::vector<Buffer*>* win_cbs, size_t buffer_len, bool sticky, std::vector<size_t>* collection);

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
		std::function<size_t(size_t)> fun_body;
		Buffer* win_cb;
		Buffer* wout_cb;

	public: 

		Worker(std::function<size_t(size_t)> fun_body, size_t buffer_len, bool sticky);

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
		Collector(std::vector<Buffer*>* wout_cbs, size_t buffer_len, bool sticky);

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
		size_t nw;
		bool sticky;
		Emitter* emitter;
		std::vector<Buffer*>* win_cbs;
		std::vector<Worker*>* workers;
		std::function<size_t(size_t)> fun_body;
		std::vector<Buffer*>* wout_cbs;
		Collector* collector;

		Worker* add_worker(size_t buffer_len);

	public:

		Autonomic_Farm(size_t nw, std::function<size_t(size_t)> fun_body, size_t buffer_len, bool sticky, std::vector<size_t>* collection);

		void run_and_wait();

		//void push(I task); <-- dipende
		//O pop();
};









