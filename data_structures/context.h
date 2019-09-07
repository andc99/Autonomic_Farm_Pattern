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
		std::deque<Worker*>* trace;	
	
	public:
		Context(size_t context_id);

		~Context();

		size_t get_context_id();

		size_t get_n_threads();

		std::deque<Worker*>* get_trace();

		void move_in(Worker* pe);

		Worker* move_out();

		size_t get_avg_ts();
};


#endif
