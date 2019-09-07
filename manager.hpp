#ifndef MANAGER_HPP 
#define MANAGER_HPP 


#include <fstream>
#include <sstream> 
#include <queue>

#include "worker.hpp"
#include "processing_element.h"
#include "./data_structures/context.h"

/////////////////////////////////////////////////////////////////////////
//
//	Manager	
//
/////////////////////////////////////////////////////////////////////////


class Manager : public ProcessingElement{
	private:
		std::ofstream data;
		std::atomic<bool>* stop;
		const size_t max_nw;
		const size_t ts_goal;
		size_t ts_upper_bound, ts_lower_bound; 
		ProcessingElement *emitter, *collector;
		std::deque<Context*> active_contexts = std::deque<Context*>();
		std::deque<Context*> idle_contexts = std::deque<Context*>();
		std::deque<Worker*> ws_queue;

		std::queue<size_t>* nw_series; 
		size_t pos = 0, sliding_size, acc = 0;


//ho provato a controllare se se un dato core ci sono altre app ma sia attraverso il throughtput sia attraverso il service time, non riesco perchè se posiziono un'app sul medesimo core di dove sta già girando la farm, tutti i contesti decrementano in modo uguale le prestazioni. Inoltre controllando da htop se aggiungo un'app diminuisce il carico su un app e viene incrementato il clock ma nonostante questo l'applicazione dell'autonous sotto controllo rimane appesantita dalla seconda. Come se ...?
		void body(){
			size_t farm_ts = 0, time = 0, rest = 200; //rand() 
			while(!(*this->stop)){
				time+=rest;
				std::this_thread::sleep_for(std::chrono::milliseconds(rest));
				this->concurrency_throttling();
				farm_ts = this->get_service_time_farm();
				std::cout << "farm_ts: " << farm_ts << std::endl;
				std::cout << " >> " << this->active_contexts.size() << std::endl;

				if(this->data.is_open())
					this->data << this->active_contexts.size() << "," << farm_ts << "," << time << "\n";
			};
			data.close();
			return;
		}


		void concurrency_throttling(){
			size_t Tw = this->get_avg_service_time_contexts();
			size_t Te = this->emitter->get_ts();
			size_t Tc = this->collector->get_ts();
			if(Tw < Te || Tw < Tc) return; //caso in cui il max non sia Tw non ci si può fare nulla
			size_t nw = this->active_contexts.size();
			if(Tw > this->ts_upper_bound){
				nw = Tw/this->ts_goal;
				nw = (nw <= this->max_nw) ? nw : this->max_nw;
			}
			else if(Tw < this->ts_lower_bound){
				nw = this->ts_goal/Tw; 
				nw = (nw < this->active_contexts.size()) ? nw : this->active_contexts.size()-1;
			}
			else if(Tw > this->ts_goal && Tw <= this->ts_upper_bound && this->detect_bottlenecks()){
				nw = Tw/this->ts_goal;
				nw = (nw <= this->max_nw) ? nw : this->max_nw;
			}
			this->update_nw_moving_avg(nw);
			this->threads_scheduling_policy(this->get_nw_moving_avg());
			return;
		}

		void wake_worker(){
			if(this->idle_contexts.size() == 0) return;
			Worker* w = NULL; 
			Context *act_context, *to_wake;
			to_wake = this->idle_contexts.front();
			this->idle_contexts.pop_front();
			act_context = this->active_contexts.back();
			w = act_context->move_out();
			to_wake->move_in(w);
			this->active_contexts.push_back(to_wake);	
			return;
		}

		void idle_worker(){ 
			if(this->active_contexts.size() == 1) return;
			Worker* w = NULL;
			Context *act_context, *to_idle_context;
			to_idle_context = this->active_contexts.back();
			this->active_contexts.pop_back();
			while(to_idle_context->get_n_threads() > 0){ 
				w = to_idle_context->move_out();
				act_context = this->active_contexts.back();
				act_context->move_in(w);
				this->active_contexts.pop_back();
				this->active_contexts.push_front(act_context); 
			}
			this->idle_contexts.push_back(to_idle_context);	
			return;
		}

		void threads_scheduling_policy(size_t new_nw){
			if(new_nw > this->active_contexts.size() &&  this->max_nw - new_nw <= this->idle_contexts.size()){
				size_t to_add = new_nw - this->active_contexts.size();
				for(auto i = 0; i < to_add; i++)
					this->wake_worker();
			}
			else if(new_nw < this->active_contexts.size() && new_nw > 0){
				size_t to_remove = this->active_contexts.size() - new_nw;
				for(auto i = 0; i < to_remove; i++)
					this->idle_worker();
			}
			this->redistribute();
		}


//il throughput per ocllector ed emitter, usiamo la varianza perchè Prendono il task e lo mettono da un'altra parte
//questo deve semplicemente essere una piccola ccortezza. Se sono in una buona posizione ma
//mi rendo conto che la coda si sta sempre più ingrossando, allora evito di riempirla (c'ho anche il bound che è già buono. Quindi magari do un piccolo aumento di worker affinchè postponga il problema. Quindi serve un range erp il quale è stabile ma che non lo faccia diventare troppo veloce. In particolare devo definire dei largini per i quali quando è stabile, se detecta un bottleneck prova ad aumentare. Quindi anche se va più veloce del ts goal ma ritarda un bottleneck, aggiungi
		bool detect_bottlenecks(){
			size_t acc = 0;
			float mean_ts_ws, ro;
			for(auto context : this->active_contexts)
				for(auto worker : *context->get_trace())
					acc += worker->get_ts();
			mean_ts_ws = static_cast<float>(acc)/this->max_nw;	
			ro = static_cast<float>(mean_ts_ws)/(this->emitter->get_ts()*this->max_nw);
			return (ro > 0.5) ? true : false;
		}

		void update_nw_moving_avg(size_t new_value){
			this->acc += new_value;
			this->nw_series->push(new_value);
			if(this->pos <= this->sliding_size)
				this->pos++;	
			else{
				this->acc -= this->nw_series->front();
				this->nw_series->pop();
			}
			return;
		}

		void redistribute(){
			size_t r = this->max_nw / this->active_contexts.size();
			for(auto context : this->active_contexts)
				this->resize(context, r);
			Worker* w = NULL;
			for(auto context : this->active_contexts){
				while(context->get_n_threads() < r){
					w = this->ws_queue.front();
					this->ws_queue.pop_front();
					context->move_in(w);
				}
			}
			Context* context;
			while(!this->ws_queue.empty()){
				w = this->ws_queue.front();
				this->ws_queue.pop_front();
				this->active_contexts.front()->move_in(w);
				context = this->active_contexts.front();
				this->active_contexts.pop_front();
				this->active_contexts.push_back(context);
			}
			return;
		}

		void resize(Context* context, size_t size){
			Worker* w;
			while(context->get_n_threads() > size){
				 w = context->move_out();
				this->ws_queue.push_front(w);
			}
			return;	
		}



	public:
		Manager(size_t ts_goal,
				std::atomic<bool>* stop,
				ProcessingElement* emitter,
				ProcessingElement* collector,
				std::vector<Worker*>* workers,
				size_t nw,
				size_t max_nw,
				size_t n_contexts,
				size_t sliding_size
				) : ts_goal(ts_goal), stop(stop), max_nw(max_nw), sliding_size(sliding_size), ProcessingElement(){

			std::ostringstream file_name_stream;
			this->nw_series =  new std::queue<size_t>();
			this->emitter = emitter;
			this->collector = collector;

			this->emitter->set_context(0);
			this->collector->set_context(1);
			this->set_context(2);

			for(auto id_context = 0; id_context < n_contexts; id_context++)
				this->idle_contexts.push_back(new Context(id_context));

			this->ts_lower_bound = this->ts_goal - this->ts_goal*2/10;
			this->ts_lower_bound = (this->ts_lower_bound < 0) ? 0 : this->ts_lower_bound;
			this->ts_upper_bound = this->ts_goal + this->ts_goal*2/10;

			Context* context;
			for(auto i = 0; i < nw; i++){
				context = this->idle_contexts.front();
				this->idle_contexts.pop_front();
				context->move_in((*workers)[i]);
				this->active_contexts.push_back(context);
			}
			for(auto i = nw; i < max_nw; i++) 
				this->active_contexts[i%nw]->move_in((*workers)[i]);

			file_name_stream << this->active_contexts.size() << "_" << this->max_nw << "_" << this->ts_goal << "_" << (*workers)[0]->get_in_buffer()->safe_get_size() << ".csv";
			this->data.open("./data/"+file_name_stream.str());
			if(this->data.is_open())
				this->data << this->ts_goal << "\n" << this->ts_upper_bound << "\n" << "Degree,Service_Time,Time\n";
			return;
		}

		~Manager(){
			delete this->stop;
			delete this->nw_series;
		}

		void run(){
			this->thread = new std::thread(&Manager::body, this);
			return;
		}


		size_t get_nw_moving_avg(){
			return this->acc/this->pos;
		}



		size_t get_avg_service_time_contexts(){
			size_t contexts_ts_avg = 0;	
			for(auto context : this->active_contexts)
				contexts_ts_avg+=context->get_avg_ts();	
			return contexts_ts_avg/=this->active_contexts.size();
		}

		size_t get_service_time_farm(){
			return std::max({this->emitter->get_ts(),
					this->collector->get_ts(),
					this->get_avg_service_time_contexts()/this->active_contexts.size()
					});
		}

		void info(){
			std::cout << "\n***************" << std::endl;
			std::cout << " ACTIVE CONTEXTS " << std::endl;
			for(auto act_context : this->active_contexts)
				std::cout << act_context->get_context_id() << "/ N° threads " << act_context->get_n_threads() << "/ Tcontext " << act_context->get_avg_ts() << std::endl;;
			std::cout << " -------- " << std::endl;
			std::cout << " IDLE CONTEXTS" << std::endl;
			for(auto idle_context : this->idle_contexts)
				std::cout << idle_context->get_context_id() << "/ N° threads " << idle_context->get_n_threads() << "/ Tcontext " << idle_context->get_avg_ts() << std::endl;;
			std::cout << " --------\n    WORKERS" << std::endl;
			for(auto const& context : this->active_contexts){
				std::deque<Worker*>* trace = context->get_trace();
				for(auto const& w : *trace){
					std::cout << "Worker " << w->get_id() << "--> Tw " << w->get_ts()  << std::endl;
				}
			}

			return;
		}


/*
//Assumo che i task siano distribuiti a caso perciò se la media di un core si distacca molto allora significa che c'è un'altra app sopra
void Manager::is_application_overlayed(){
	long diff = 0;
	for(auto context : this->active_contexts){
		diff = context->get_avg_ts() - this->get_contexts_avg_ts();
		long threshold = this->get_ts()*50/100;
		if( diff > 0 && diff > threshold)
			this->transfer_threads_to_idle_core(context);	
		this->get_avg_service_time_contexts();
	}
}

void Manager::transfer_threads_to_idle_core(Context*& from){
	if(this->idle_contexts.size() == 0)
		return;

	Context *swap, **to;
	std::deque<Worker*> *from_trace, *to_trace;

	from_trace = from->get_trace();
	to = &this->idle_contexts.front();
	to_trace = (*to)->get_trace();


	for(Worker* w : *from_trace)
		w->set_context((*to)->get_context_id());	
	from_trace->swap(*to_trace);
	
	swap = from;
	from = *to;
	*to = swap;

	return;
}


void Manager::set_contexts_avg_ts(size_t new_value){
	this->contexts_avg_ts = new_value;
	return;
}

long Manager::get_contexts_avg_ts(){
	return this->contexts_avg_ts;
}

*/

}; 

#endif
