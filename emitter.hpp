#ifndef EMITTER_HPP 
#define EMITTER_HPP 

#include "processing_element.h"
#include "worker.hpp"

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

		void body(){
			size_t act_service_time = 0, act_context_id = 0;
			for(auto i = 0; i < (*collection).size(); i++){
				this->start_time = std::chrono::high_resolution_clock::now();
				ssize_t &task = (*collection)[i];
				this->next_push(&task);
				if ( (act_context_id = this->get_context() ) != sched_getcpu())
					move_to_context(act_context_id);
				this->end_time = std::chrono::high_resolution_clock::now();
				act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(this->end_time - this->start_time).count();
			}
			for(auto i = 0; i < this->n_buffers; i++)
				this->next_push(EOS);
			return;
		}

		std::function<void(void*)> rotate_push(std::vector<BUFFER*>* buffers){
			size_t id_queue = 0;
			return [id_queue, buffers](void* task) mutable {
				((*buffers)[id_queue]->BUFFER::safe_push)(task);
				(id_queue < buffers->size()-1) ? id_queue++ : id_queue = 0;	
			};
		}


	public:

		Emitter(std::vector<BUFFER*>* win_bfs, size_t buffer_len, std::vector<ssize_t>* collection) : n_buffers(win_bfs->size()), ProcessingElement(){
			this->next_push = Emitter::rotate_push(win_bfs);
			this->collection = collection;
		}


		void run(){
			this->thread = new std::thread(&Emitter::body, this);
			return;
		}


};

#endif
