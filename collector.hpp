#ifndef COLLECTOR_HPP
#define COLLECTOR_HPP

#include "processing_element.h"

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

		void body(){
			void* task = 0;
			size_t act_service_time = 0, act_context_id = 0, eos_counter = 0;
			while(eos_counter < this->n_buffers){ 
				this->start_time = std::chrono::high_resolution_clock::now();
				this->next_pop(&task);
				eos_counter += (task == EOS) ? 1 : 0;
				ssize_t &t = (*((ssize_t*) task));
				this->collector_buffer->safe_push(&t); 
				if ( (act_context_id = this->get_context() ) != sched_getcpu())
					move_to_context(act_context_id);
				this->end_time = std::chrono::high_resolution_clock::now();
				act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(this->end_time - this->start_time).count();
				this->update_ts(act_service_time);
			};
			while(!(this->collector_buffer->safe_push(EOS))) continue;
			return;
		}


		std::function<void(void**)> rotate_pop(std::vector<BUFFER*>* buffers){
			size_t id_queue{0};
			return [id_queue, buffers](void** task) mutable {
				((*buffers)[id_queue]->BUFFER::safe_pop)(task);
				(id_queue < buffers->size()-1) ? id_queue++ : id_queue = 0;	
			};
		}


	public:
		Collector(std::vector<BUFFER*>* wout_bfs, size_t buffer_len) : n_buffers(wout_bfs->size()), ProcessingElement(){
			this->next_pop = Collector::rotate_pop(wout_bfs);
			this->collector_buffer = new BUFFER(buffer_len); 
		}
		void run(){
			this->thread = new std::thread(&Collector::body, this);
			return;
		}

		BUFFER* get_out_buffer(){
			return this->collector_buffer;
		}

};


#endif
