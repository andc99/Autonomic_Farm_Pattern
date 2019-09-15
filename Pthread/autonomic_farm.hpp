#ifndef AUTONOMIC_FARM_HPP
#define AUTONOMIC_FARM_HPP

#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <vector>
#include <thread>

#include "processing_element.h"
#include "emitter.hpp"
#include "worker.hpp"
#include "collector.hpp"
#include "manager.hpp"


/////////////////////////////////////////////////////////////////////////
//
//	Autonomic Farm	
//
/////////////////////////////////////////////////////////////////////////

class Autonomic_Farm{
	private:
		const std::function<size_t(size_t)> fun_body; //Worker function
		size_t max_nw;
		std::atomic<bool>* stop; //to stop the manager
		Manager* manager;
		Emitter* emitter;
		std::vector<Worker*>* workers;
		Collector* collector;


		//Creates workers and insert them buffers in two vector (win_bfs, wout_bfs) to be passed to emitter and collector
		Worker* add_worker(std::vector<BUFFER*>* win_bfs, std::vector<BUFFER*>* wout_bfs, size_t buffer_len){ 
			Worker* worker = new Worker(this->fun_body, buffer_len);
			win_bfs->push_back(worker->get_in_buffer());
			wout_bfs->push_back(worker->get_out_buffer());
			this->workers->push_back(worker);
			return worker;
		}


	public:

		Autonomic_Farm(size_t ts_goal, size_t nw, size_t max_nw, std::function<size_t(size_t)> fun_body, size_t buffer_len, std::vector<size_t>* collection, size_t sliding_size = 100) : fun_body(fun_body){ 
			if(nw > max_nw) nw = max_nw;
			if(ts_goal < 1) ts_goal = 1;

			size_t n_contexts = std::thread::hardware_concurrency();

			//Following instruction avoid wrong arguments
			nw = (nw <= n_contexts) ? nw : n_contexts; 
			this->max_nw = (max_nw <= n_contexts) ? max_nw : n_contexts;

			std::cout << "Initial_degree: " << nw << std::endl;
			std::cout << "Max_degree " << this->max_nw << std::endl;

			this->stop = new std::atomic<bool>(false);

			//This vector will be shared with the Emitter so to distribute tasks among all workers
			std::vector<BUFFER*>* win_bfs = new std::vector<BUFFER*>();

			this->workers = new std::vector<Worker*>();

			//This vector will be shared with the Collector so to collect tasks among all workers
			std::vector<BUFFER*>* wout_bfs = new std::vector<BUFFER*>();	

			//Create max number of workers
			for(auto i = 0; i < this->max_nw; i++) 
				this->add_worker(win_bfs, wout_bfs, buffer_len);

			this->emitter = new Emitter(win_bfs, buffer_len, collection);
			this->collector = new Collector(wout_bfs, buffer_len);

			this->manager = new Manager(ts_goal, this->stop,
					this->emitter, this->collector, this->workers,
					nw, this->max_nw, n_contexts, sliding_size);
		}

		void run(){
			this->manager->run();
			this->emitter->run();
			for(auto worker : *this->workers)
				worker->run();	
			this->collector->run();	
			return;
		}

		void join(){
			for(auto worker : *this->workers)
				worker->join();	
			this->collector->join();
			*this->stop = true;
			this->manager->join();
			return;
		}


		size_t pop_outputs(){
			void* task = 0;
			this->collector->get_out_buffer()->safe_pop(&task);
			size_t &t = (*((size_t*) task));
			return task == EOS ? -1 : t;
		}

};



#endif
