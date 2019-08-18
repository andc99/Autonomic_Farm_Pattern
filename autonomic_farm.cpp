#include "autonomic_farm.h"


/////////////////////////////////////////////////////////////////////////
//
//	ProcessingElement	
//
/////////////////////////////////////////////////////////////////////////


ProcessingElement::ProcessingElement(bool sticky){
	this->sticky = sticky;
	static std::atomic<size_t> id{0};
	this->thread_id = id++; 
	this->context_id_lock = new std::mutex();
}

ProcessingElement::~ProcessingElement(){ //bypassabile per via dela join
	delete thread;
	delete this->context_id_lock;
	return;
}

void ProcessingElement::join(){
	this->thread->join();
	return;
}

size_t ProcessingElement::get_id(){ 
	return this->thread_id;
}

void ProcessingElement::set_context(){
	std::lock_guard<std::mutex> lock(*context_id_lock);
	this->context_id = sched_getcpu();
	return;
}

size_t ProcessingElement::get_context(){
	std::lock_guard<std::mutex> lock(*context_id_lock);
	return this->context_id;
}

ssize_t ProcessingElement::move_to_context(size_t id_context){
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(static_cast<int>(id_context)%static_cast<int>(std::thread::hardware_concurrency()), &cpuset); //module %
	ssize_t error = pthread_setaffinity_np(this->thread->native_handle(), sizeof(cpu_set_t), &cpuset);
	if (error != 0)
		std::cout << "Error calling pthread_setaffinity_np: " << error << "\n";
	return error;
}

long ProcessingElement::get_mean_service_time(){
	return this->mean_service_time;
}

long ProcessingElement::get_variance_service_time(){
	return this->variance_service_time;
}

void ProcessingElement::update_stats(long act_service_time){
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

Emitter::Emitter(std::vector<Buffer*>* win_cbs, size_t buffer_len, bool sticky, std::vector<ssize_t>* collection) : ProcessingElement(sticky){
	this->win_cbs = win_cbs;
	this->emitter_cb = new BUFFER(buffer_len); 
	this->collection = collection;
}

void Emitter::body(){
	size_t id_queue{0};
	long act_service_time = 0;
	std::chrono::high_resolution_clock::time_point start_time, end_time;
	if(this->sticky){move_to_context(this->get_id());}
	for(size_t i = 0; i < (*collection).size(); i++){
		start_time = std::chrono::high_resolution_clock::now();
		ssize_t &task = (*collection)[i];
		(*win_cbs)[id_queue]->safe_push(&task); //safe_try!!!!
		id_queue = (++id_queue)%win_cbs->size();	
		end_time = std::chrono::high_resolution_clock::now();
		act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
		this->update_stats(act_service_time);
	}
	for(auto win_cb : *(this->win_cbs))
		win_cb->safe_push(EOS); // qui non ci può andare la safe_try
	return;
}

void Emitter::run(){
	this->thread = new std::thread(&Emitter::body, this);
	return;
}

Buffer* Emitter::get_in_queue(){
	return this->emitter_cb;
}


/////////////////////////////////////////////////////////////////////////
//
//	Worker	
//
/////////////////////////////////////////////////////////////////////////


Worker::Worker(std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len, bool sticky):ProcessingElement(sticky){
	this->fun_body = fun_body;
	this->win_cb = new BUFFER(buffer_len); 
	this->wout_cb = new BUFFER(buffer_len); 
};

void Worker::body(){
	void* task = 0;
	long act_service_time = 0;
	std::chrono::high_resolution_clock::time_point start_time, end_time;
	if(this->sticky){move_to_context(this->get_id());}
	win_cb->safe_pop(&task);
	while( task != EOS){ 
		start_time = std::chrono::high_resolution_clock::now();
		ssize_t &t = (*((ssize_t*) task));
		t = (fun_body(t));
		this->wout_cb->safe_push(&t); //safe_try
		this->win_cb->safe_pop(&task);	
		end_time = std::chrono::high_resolution_clock::now();
		act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
		this->update_stats(act_service_time);
	};
	this->wout_cb->safe_push(EOS);
	return;
}

void Worker::run(){
	this->thread = new std::thread(&Worker::body, this);
	return;
}


Buffer* Worker::get_in_queue(){
	return this->win_cb;
}

Buffer* Worker::get_out_queue(){
	return this->wout_cb;
}

/////////////////////////////////////////////////////////////////////////
//
//	Collector	
//
/////////////////////////////////////////////////////////////////////////

Collector::Collector(std::vector<Buffer*>* wout_cbs, size_t buffer_len, bool sticky):ProcessingElement(sticky){
	this->wout_cbs = wout_cbs;
	this->collector_cb = new BUFFER(buffer_len); 
}

void Collector::body(){
	void* task = 0;
	size_t id_queue{0};
	long act_service_time = 0;
	std::chrono::high_resolution_clock::time_point start_time, end_time;
	if(this->sticky){move_to_context(this->get_id());}
	(*wout_cbs)[id_queue]->safe_pop(&task);
	size_t eos_counter = (task == EOS) ? 1 : 0;
	while(eos_counter < (*wout_cbs).size()){ 
		start_time = std::chrono::high_resolution_clock::now();
		ssize_t &t = (*((ssize_t*) task));
		//this->collector_cb->safe_push(&t); //safe_try
		id_queue = (++id_queue)%wout_cbs->size();
		(*wout_cbs)[id_queue]->safe_pop(&task);
		eos_counter += (task == EOS) ? 1 : 0;
		end_time = std::chrono::high_resolution_clock::now();
		act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
		this->update_stats(act_service_time);
	};
	this->collector_cb->safe_push(EOS);
	return;
}

void Collector::run(){
	this->thread = new std::thread(&Collector::body, this);
	return;
}

Buffer* Collector::get_out_queue(){
	return this->collector_cb;
}

/////////////////////////////////////////////////////////////////////////
//
//	Autonomic Farm	
//
/////////////////////////////////////////////////////////////////////////

Worker* Autonomic_Farm::add_worker(size_t buffer_len){ //c'è da runnarlo poi ehhhh
	Worker* worker = new Worker(this->fun_body, buffer_len, this->sticky);
	this->win_cbs->push_back(worker->get_in_queue());
	this->wout_cbs->push_back(worker->get_out_queue());
	(*this->workers).push_back(worker);
	return worker;
}

//da lockcare e capire quale sia il service time più giusto
/*
long Autonomic_Farm::get_service_time_farm(){
	return std::max({this->emitter.
}
*/



Autonomic_Farm::Autonomic_Farm(size_t nw, size_t max_nw, std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len, bool sticky, std::vector<ssize_t>* collection){ 
	this->nw = nw;
	this->max_nw = max_nw;
	this->sticky = sticky;
	this->fun_body = fun_body;
	this->win_cbs = new std::vector<Buffer*>();
	this->workers = new std::vector<Worker*>();
	this->wout_cbs = new std::vector<Buffer*>();
	this->emitter = new Emitter(this->win_cbs, buffer_len, this->sticky, collection);
	this->collector = new Collector(this->wout_cbs, buffer_len, this->sticky);
	for(size_t i = 0; i < nw; i++) //vanno possibilmente su core distinti tra loro
		this->add_worker(buffer_len);
}

void Autonomic_Farm::run_and_wait(){
	this->emitter->run();
	for(auto worker : (*this->workers))
		worker->run();	
	this->collector->run();
	this->collector->join();
	std::cout << "Emitter: " << this->emitter->get_mean_service_time() << " - " << this->emitter->get_variance_service_time() << std::endl;
	for(size_t i = 0; i < nw; i++)
		std::cout << "Worker " << (*this->workers)[i]->get_id() << ": " << (*this->workers)[i]->get_mean_service_time() << " - " << (*this->workers)[i]->get_variance_service_time() << std::endl;
	std::cout << "Collector: " << this->collector->get_mean_service_time() << " - " << this->collector->get_variance_service_time() << std::endl;
}


