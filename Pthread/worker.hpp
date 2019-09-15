
#ifndef WORKER_HPP
#define WORKER_HPP

#include "processing_element.h"


//////////////////////////////////////////////////////////////////////////
//
//	Worker	
//
/////////////////////////////////////////////////////////////////////////

class Worker : public ProcessingElement{
	private:
		std::function<size_t(size_t)> fun_body;
		BUFFER* win_bf;
		BUFFER* wout_bf;

		void body(){
			void* task = 0;
			size_t act_service_time = 0, act_context_id = 0;
			this->win_bf->safe_pop(&task);
			while( task != EOS){ 
				this->start_time = std::chrono::high_resolution_clock::now();
				ssize_t &t = (*((ssize_t*) task));
				t = (fun_body(t));
				this->wout_bf->safe_push(&t); 
				this->win_bf->safe_pop(&task);	
				if ( (act_context_id =this->get_context() ) != sched_getcpu())
					move_to_context(act_context_id);
				this->end_time = std::chrono::high_resolution_clock::now();
				act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(this->end_time - this->start_time).count() + 1;
				this->update_ts(act_service_time);
			};
			this->wout_bf->safe_push(EOS);
			return;
		}


	public: 

		Worker(std::function<size_t(size_t)> fun_body, size_t buffer_len) : ProcessingElement(){
			this->fun_body = fun_body;
			this->win_bf = new BUFFER(buffer_len); 
			this->wout_bf = new BUFFER(buffer_len); 
		};

		void run(){
			this->thread = new std::thread(&Worker::body, this);
			return;
		}

		BUFFER* get_in_buffer(){
			return this->win_bf;
		}

		BUFFER* get_out_buffer(){
			return this->wout_bf;
		}

};


#endif
