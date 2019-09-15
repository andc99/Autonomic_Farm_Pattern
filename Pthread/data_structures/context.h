#ifndef CONTEXT_H
#define CONTEXT_H

#include <cstddef>
#include <deque>

#include "./../worker.hpp" 

/////////////////////////////////////////////////////////////////////////
//
//	Context	
//
/////////////////////////////////////////////////////////////////////////
class Context{
	private:
		const size_t context_id;
		std::deque<Worker*>* trace; //trace all workers con this context_id 
	
	public:
		Context(size_t context_id);

		~Context();

		size_t get_context_id();

		//return the number of threads in this context
		size_t get_n_threads();

		std::deque<Worker*>* get_trace();

		//move the worker w on this context
		void move_in(Worker* w);

		//remove a worker from this context
		Worker* move_out();

		//get the average service time of all workers in this context
		size_t get_avg_ts();
};


#endif
