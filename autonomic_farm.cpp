#include "autonomic_farm.h"


/////////////////////////////////////////////////////////////////////////
//
//	ProcessingElement	
//
/////////////////////////////////////////////////////////////////////////


ProcessingElement::ProcessingElement(){
	static std::atomic<size_t> id{0};
	this->thread_id = id++; 
	this->context_id_lock = new std::mutex();
	this->stats_lock = new std::mutex();
}

ProcessingElement::~ProcessingElement(){ 
	delete thread;
	delete this->context_id_lock;
	delete this->stats_lock;
	return;
}

void ProcessingElement::join(){
	this->thread->join();
	return;
}

size_t ProcessingElement::get_id(){ 
	return this->thread_id;
}


size_t ProcessingElement::get_context(){
	std::lock_guard<std::mutex> lock(*context_id_lock);
	return this->context_id;
}


void ProcessingElement::set_context(size_t context_id){
	std::lock_guard<std::mutex> lock(*context_id_lock);
	this->context_id = context_id;
	return;
}

int ProcessingElement::move_to_context(size_t id_context){
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(static_cast<int>(id_context)%static_cast<int>(std::thread::hardware_concurrency()), &cpuset); 
	int error = pthread_setaffinity_np(this->thread->native_handle(), sizeof(cpu_set_t), &cpuset);
	if (error != 0)
		std::cout << "Error calling pthread_setaffinity_np: " << error << "\n";
	else
		this->set_context(id_context); 
	return error;
}

long ProcessingElement::get_mean_service_time(){
	std::lock_guard<std::mutex> lock(*stats_lock);
	return this->mean_service_time;
}

long ProcessingElement::get_variance_service_time(){
	std::lock_guard<std::mutex> lock(*stats_lock);
	return this->variance_service_time;
}

void ProcessingElement::update_stats(long act_service_time){
	std::lock_guard<std::mutex> lock(*stats_lock);
	size_t pred_mean_service_time = this->mean_service_time;
	this->processed_elements++;
	this->update_mean_service_time(act_service_time);
	this->update_variance_service_time(act_service_time, pred_mean_service_time);
	return;
}

long ProcessingElement::update_mean_service_time(long act_service_time){
	this->mean_service_time = this->mean_service_time + (act_service_time - this->mean_service_time)/(this->processed_elements);
	return this->mean_service_time;
}

long ProcessingElement::update_variance_service_time(long act_service_time, long pred_mean_service_time){
	long sn = this->variance_service_time + (act_service_time - pred_mean_service_time)*(act_service_time - this->mean_service_time);
	this->variance_service_time = sqrt((sn/this->processed_elements));
	return this->variance_service_time;
}

/////////////////////////////////////////////////////////////////////////
//
//	Emitter
//
/////////////////////////////////////////////////////////////////////////



Emitter::Emitter(std::vector<BUFFER*>* win_bfs, size_t buffer_len, std::vector<ssize_t>* collection) : n_buffers(win_bfs->size()), ProcessingElement(){

	this->next_push = Emitter::rotate_push(win_bfs);
	this->emitter_buffer = new BUFFER(buffer_len); 
	this->collection = collection;
}

void Emitter::body(){
	long act_service_time = 0;
	std::chrono::high_resolution_clock::time_point start_time, end_time;
	for(auto i = 0; i < (*collection).size(); i++){
		start_time = std::chrono::high_resolution_clock::now();
		ssize_t &task = (*collection)[i];
		this->next_push(&task);
		if (this->get_context() != sched_getcpu())
			move_to_context(get_context());
		end_time = std::chrono::high_resolution_clock::now();
		act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
		this->update_stats(act_service_time);
	}
	for(auto i = 0; i < this->n_buffers; i++){
		this->next_push(EOS);
	}
	return;
}

void Emitter::run(){
	this->thread = new std::thread(&Emitter::body, this);
	return;
}

//in case stream
BUFFER* Emitter::get_in_buffer(){
	return this->emitter_buffer;
}


/////////////////////////////////////////////////////////////////////////
//
//	Worker	
//
/////////////////////////////////////////////////////////////////////////


Worker::Worker(std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len):ProcessingElement(){
	this->fun_body = fun_body;
	this->win_bf = new BUFFER(buffer_len); 
	this->wout_bf = new BUFFER(buffer_len); 
};

void Worker::body(){
	void* task = 0;
	long act_service_time = 0;
	std::chrono::high_resolution_clock::time_point start_time, end_time;
	this->win_bf->safe_pop(&task);
	while( task != EOS){ 
		start_time = std::chrono::high_resolution_clock::now();
		ssize_t &t = (*((ssize_t*) task));
		t = (fun_body(t));
		this->wout_bf->safe_push(&t); 
		this->win_bf->safe_pop(&task);	
		if (this->get_context() != sched_getcpu())
			move_to_context(this->get_context());
		end_time = std::chrono::high_resolution_clock::now();
		act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
		this->update_stats(act_service_time);
	};
	this->wout_bf->safe_push(EOS);
	return;
}

void Worker::run(){
	this->thread = new std::thread(&Worker::body, this);
	return;
}


BUFFER* Worker::get_in_buffer(){
	return this->win_bf;
}

BUFFER* Worker::get_out_buffer(){
	return this->wout_bf;
}

/////////////////////////////////////////////////////////////////////////
//
//	Collector	
//
/////////////////////////////////////////////////////////////////////////

Collector::Collector(std::vector<BUFFER*>* wout_bfs, size_t buffer_len) : n_buffers(wout_bfs->size()), ProcessingElement(){
	this->next_pop = Collector::rotate_pop(wout_bfs);
	this->collector_buffer = new BUFFER(buffer_len); 
}

void Collector::body(){
	void* task = 0;
	long act_service_time = 0;
	size_t eos_counter = 0;
	std::chrono::high_resolution_clock::time_point start_time, end_time;
	while(eos_counter < this->n_buffers){ 
		start_time = std::chrono::high_resolution_clock::now();
		this->next_pop(&task);
		eos_counter += (task == EOS) ? 1 : 0;
		ssize_t &t = (*((ssize_t*) task));
		this->collector_buffer->safe_push(&t); 
		if (this->get_context() != sched_getcpu())
			move_to_context(get_context());
		end_time = std::chrono::high_resolution_clock::now();
		act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
		this->update_stats(act_service_time);
	};
	this->collector_buffer->safe_push(EOS);
	return;
}

void Collector::run(){
	this->thread = new std::thread(&Collector::body, this);
	return;
}

BUFFER* Collector::get_out_buffer(){
	return this->collector_buffer;
}


/////////////////////////////////////////////////////////////////////////
//
//	Context	
//
/////////////////////////////////////////////////////////////////////////

Context::Context(size_t context_id) : context_id(context_id){};

size_t Context::get_context_id(){
	return this->context_id;
}

size_t Context::get_n_threads(){
	return this->trace.size();
}

std::deque<Worker*>* Context::get_trace(){
	return &this->trace;
}

void Context::move_in(Worker* w){
	this->trace.push_back(w);
	w->set_context(this->get_context_id());
	return;
}

Worker* Context::move_out(){
	Worker* w = this->trace.front(); 
	this->trace.pop_front();
	return w;
}

/////////////////////////////////////////////////////////////////////////
//
//	Manager	
//
/////////////////////////////////////////////////////////////////////////


void Manager::wake_workers(size_t n){
	n = (n <= this->max_nw-this->nw) ? n : this->max_nw-this->nw;
	n = (n <= this->idle.size()) ? n : this->idle.size();
	Worker* w = NULL; 
	Context *act_context, *to_wake;
	for(auto i = 0; i < n; i++){
		to_wake = this->idle.front();
		this->idle.pop_front();
		act_context = this->active_contexts.front();
		this->active_contexts.pop_front();
		w = act_context->move_out();
		to_wake->move_in(w);
		this->active_contexts.push_front(to_wake);	
		this->active_contexts.push_back(act_context);
		this->nw++;
	}	
	this->redistribute();
	return;
}

void Manager::idle_workers(size_t n){ 
	n = (n < this->active_contexts.size()) ? n : this->active_contexts.size()-1; //ne lascio almen un sveglio
	Worker* w = NULL;
	Context *act_context, *to_idle_context;
	for(auto i = 0; i < n; i++){
		to_idle_context = this->active_contexts.back();
		this->active_contexts.pop_back();
		while(to_idle_context->get_n_threads() > 0){ 
			w = to_idle_context->move_out();
			act_context = this->active_contexts.front();
			act_context->move_in(w);
			this->active_contexts.pop_front();
			this->active_contexts.push_back(act_context); 
			
		}
		this->idle.push_back(to_idle_context);	
		this->nw--;
	}
	this->redistribute();
	return;
}


long Manager::get_service_time_farm(){
	long mean_service_time_workers = 0;
	for(auto context : this->active_contexts) //a sto punto ce li ho divisi per contesto ehh ottimizzazioni?
		for(auto worker : *(context->get_trace()))
			mean_service_time_workers+=worker->get_mean_service_time();
	mean_service_time_workers/=static_cast<long>(this->max_nw);
	return std::max({this->emitter->get_mean_service_time(),
			this->collector->get_mean_service_time(),
			mean_service_time_workers/static_cast<long>(this->nw)});
}



//il throughput per ocllector ed emitter, usiamo la varianza perchè Prendono il task e lo mettono da un'altra parte
Manager::Manager(long ts_goal,
		std::atomic<bool>* stop,
		Emitter* emitter,
		Collector* collector,
		std::vector<Worker*>* workers,
		size_t nw,
		size_t max_nw,
		size_t n_contexts) : ts_goal(ts_goal), stop(stop), nw(nw), max_nw(max_nw), ProcessingElement(){
	std::ostringstream file_name_stream;
	this->emitter = emitter;
	this->collector = collector;
	for(auto id_context = 0; id_context < n_contexts; id_context++)
		this->idle.push_back(new Context(id_context));
	//this->wake_worker(collector); // deve andare sopra l'emitter
	//this->wake_worker(this); // deve andare sopra l'emitter
	Context* context;
	for(auto i = 0; i < nw; i++){
		context = this->idle.front();
		this->idle.pop_front();
		context->move_in((*workers)[i]);
		this->active_contexts.push_back(context);
	}
	for(auto i = nw; i < max_nw; i++) 
		this->active_contexts[i%nw]->move_in((*workers)[i]);

	file_name_stream << this->nw << "_" << this->max_nw << "_" << this->ts_goal << "_" << (*workers)[0]->get_in_buffer()->safe_get_size() << ".csv";
	this->data.open("./data/"+file_name_stream.str());
	if(this->data.is_open())
		this->data << this->ts_goal << "\n" << "Degree,Service_Time,Time\n";
	return;
}

//devo poi aggiornare gli active e gli idle!
void Manager::transfer_threads_to_core(Context* from, Context* to){
	Worker* w = NULL;
	while(!from->get_trace()->empty()){
		w = from->move_out();
		to->move_in(w);
	}
	return;
}

void Manager::redistribute(){
	size_t r = this->max_nw / this->nw;
	size_t m = this->max_nw % this->nw;
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

void Manager::resize(Context* context, size_t size){
	while(context->get_n_threads() > size){
		Worker* w = context->move_out();
		this->ws_queue.push_front(w);
	}
	return;	
}

void Manager::info(){
	std::cout << "\n***************" << std::endl;
	std::cout << " ACTIVE " << std::endl;
	for(auto i = 0; i < this->active_contexts.size(); i++)
		std::cout << this->active_contexts[i]->get_context_id() << " - " << this->active_contexts[i]->get_n_threads() << std::endl;
	std::cout << " IDLE " << std::endl;
	for(auto i = 0; i < this->idle.size(); i++)
		std::cout << this->idle[i]->get_context_id() << " - " << this->idle[i]->get_n_threads() << std::endl;
	std::cout << " -------- " << std::endl;

	for(auto const& context : active_contexts){
		std::deque<Worker*>* trace = context->get_trace();
		for(auto const& w : *trace){
			std::cout << "Worker " << w->get_id() << ": " << w->get_mean_service_time() << " - " << w->get_variance_service_time() << std::endl;
		}
	}

	return;
}

void Manager::body(){
	std::chrono::high_resolution_clock::time_point start_time, end_time;
	long act_service_time;
	size_t time = 0;
	while(!(*this->stop)){
		std::cout << " >> " << nw << std::endl;
		size_t rest =  rand() % 1200 + 200;
		time+=rest;
		std::this_thread::sleep_for(std::chrono::milliseconds(rest));
		start_time = std::chrono::high_resolution_clock::now();
				//for(auto const& [key,val] : this->threads_trace){
		//	for(auto pe : val)
		//		std::cout << "ID: " << pe->get_id() << " --- " << pe->get_context() << std::endl;
		//}
		/*		if(pe->get_in_queue()->is_bottleneck())
						this->increase_degree();
		}*/
		info();
		long act_ts = this->get_service_time_farm();
		std::cout << " Service_Time " << act_ts << "\n" << std::endl;
		if(this->data.is_open())
			this->data << this->nw << "," << act_ts << "," << time << "\n";
		if( act_ts > this->ts_goal){	
			size_t n = act_ts/ts_goal; //devono esserci totali n! oppure devono essere aggiunti n?
			if(n < 0){ std::cout << "MALE " << std::endl; return;}
			this->wake_workers(n);
		}else if(act_ts < this->ts_goal*95/100){
			size_t n = this->ts_goal*95/100/act_ts; //devono esserci totali n! oppure devono essere aggiunti n?
			this->idle_workers(n);
		}else{
			std::cout << "stable" << std::endl;
		}
		end_time = std::chrono::high_resolution_clock::now();
		act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
		this->update_stats(act_service_time);
	};
	data.close();
	return;

}

void Manager::run(){
	this->thread = new std::thread(&Manager::body, this);
	return;
}

/////////////////////////////////////////////////////////////////////////
//
//	Autonomic Farm	
//
/////////////////////////////////////////////////////////////////////////


Worker* Autonomic_Farm::add_worker(std::vector<BUFFER*>* win_bfs, std::vector<BUFFER*>* wout_bfs, size_t buffer_len){ 
	Worker* worker = new Worker(this->fun_body, buffer_len);
	win_bfs->push_back(worker->get_in_buffer());
	wout_bfs->push_back(worker->get_out_buffer());
	(*this->workers).push_back(worker);
	return worker;
}

Autonomic_Farm::Autonomic_Farm(long ts_goal, size_t nw, size_t max_nw, std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len, std::vector<ssize_t>* collection) : ts_goal(ts_goal), fun_body(fun_body){ 
	if(nw > max_nw){
		std::cout << "Error nw > max_nw" << std::endl;
		return;
	}
	if(ts_goal == 0){
		std::cout << "ts_goal is 0" << std::endl;
		return;
	}
	size_t n_contexts = std::thread::hardware_concurrency();
	nw = (nw < n_contexts) ? nw : n_contexts; 
	this->max_nw = (max_nw < n_contexts) ? max_nw : n_contexts;
	std::cout << "Initial_degree: " << nw << std::endl;
	std::cout << "Max_degree " << this->max_nw << std::endl;
	this->stop = new std::atomic<bool>(false);
	std::vector<BUFFER*>* win_bfs = new std::vector<BUFFER*>();
	this->workers = new std::vector<Worker*>();
	std::vector<BUFFER*>* wout_bfs = new std::vector<BUFFER*>();	

	for(auto i = 0; i < this->max_nw; i++) 
		this->add_worker(win_bfs, wout_bfs, buffer_len);

	this->emitter = new Emitter(win_bfs, buffer_len, collection);
	this->collector = new Collector(wout_bfs, buffer_len);

	this->manager = new Manager(this->ts_goal,
			this->stop, this->emitter,
			this->collector, this->workers,
			nw, this->max_nw, n_contexts);
}

void Autonomic_Farm::run(){
	this->emitter->run();
	for(auto worker : (*this->workers))
		worker->run();	
	this->manager->run();
	this->collector->run();	
	return;
}

void Autonomic_Farm::join(){
	for(auto worker : (*this->workers))
		worker->join();	
	this->collector->join();
	*(this->stop) = true;
	this->manager->join();
	return;
}

/* altrimenti devo gestire il caso togliendo le push del collector
void Autonomic_Farm::run_and_wait(){
	this->run();
	this->join();
}
*/

size_t Autonomic_Farm::pop_outputs(){
	void* task = 0;
	this->collector->get_out_buffer()->safe_pop(&task);
	size_t &t = (*((size_t*) task));
	return task == EOS ? -1 : t;
}
