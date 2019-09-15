#ifndef PROCESSING_ELEMENT_H
#define PROCESSING_ELEMENT_H

#include <thread>
#include <mutex>
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
		std::thread* thread;
		std::mutex *context_id_lock, *stats_lock;
		size_t thread_id, context_id = 0;

		std::chrono::high_resolution_clock::time_point start_time, end_time;
		size_t service_time = 1;	

		virtual void body() = 0;
		virtual void run() = 0;

		ProcessingElement();

		~ProcessingElement();

		void update_ts(size_t act_service_time); //synched function to update the service time

		int move_to_context(size_t id_context); //function to move threads between context

	public:
		void join();
		
		size_t get_id();

		size_t get_context();

		size_t get_ts(); //synched fun to get the service time of the processing element

		void set_context(size_t context_id); //synched fun to set the context of the processing element


};


#endif
