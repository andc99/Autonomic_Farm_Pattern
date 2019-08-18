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
}

ProcessingElement::~ProcessingElement(){ //bypassabile per via dela join
	delete thread;
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
	return sched_getcpu();
}

size_t ProcessingElement::move_to_context(size_t id_context){
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(id_context%std::thread::hardware_concurrency(), &cpuset); //module %
	size_t error = pthread_setaffinity_np(this->thread->native_handle(), sizeof(cpu_set_t), &cpuset);
	if (error != 0)
		std::cout << "Error calling pthread_setaffinity_np: " << error << "\n";
	return error;
}


/////////////////////////////////////////////////////////////////////////
//
//	Emitter
//
/////////////////////////////////////////////////////////////////////////

Emitter::Emitter(std::vector<BUFFER*>* win_cbs, size_t buffer_len, bool sticky, std::vector<size_t>* collection) : ProcessingElement(sticky){
	this->win_cbs = win_cbs;
	this->emitter_cb = new BUFFER(buffer_len); 
	this->collection = collection;
}

void Emitter::body(){
	size_t id_queue{0};
	if(this->sticky){move_to_context(this->get_id());}
	for(size_t i = 0; i < (*collection).size(); i++){
		size_t &task = (*collection)[i];
		while(!(*win_cbs)[id_queue]->safe_push(&task)) //safe_try!!!!
			id_queue = (++id_queue)%win_cbs->size();	
	}
	for(auto win_cb : *(this->win_cbs))
		while(!win_cb->safe_push(EOS)) continue; // qui non ci può andare la safe_try
	return;
}

void Emitter::run(){
	this->thread = new std::thread(&Emitter::body, this);
	return;
}

BUFFER* Emitter::get_in_queue(){
	return this->emitter_cb;
}


/////////////////////////////////////////////////////////////////////////
//
//	Worker	
//
/////////////////////////////////////////////////////////////////////////


Worker::Worker(std::function<size_t(size_t)> fun_body, size_t buffer_len, bool sticky):ProcessingElement(sticky){
	this->fun_body = fun_body;
	this->win_cb = new BUFFER(buffer_len); 
	this->wout_cb = new BUFFER(buffer_len); 
};

void Worker::body(){
	void* task = 0;
	if(this->sticky){move_to_context(this->get_id());}
	while(!win_cb->safe_pop(&task)) continue;
	while( task != EOS){ 
		size_t &t = (*((size_t*) task));
		t = (fun_body(t));
		while(!this->wout_cb->safe_push(&t)) continue; //safe_try
		while(!this->win_cb->safe_pop(&task)) continue;	
	};
	while(!this->wout_cb->safe_push(EOS)) continue;
	return;
}

void Worker::run(){
	this->thread = new std::thread(&Worker::body, this);
	return;
}


BUFFER* Worker::get_in_queue(){
	return this->win_cb;
}

BUFFER* Worker::get_out_queue(){
	return this->wout_cb;
}

/////////////////////////////////////////////////////////////////////////
//
//	Collector	
//
/////////////////////////////////////////////////////////////////////////

Collector::Collector(std::vector<BUFFER*>* wout_cbs, size_t buffer_len, bool sticky):ProcessingElement(sticky){
	this->wout_cbs = wout_cbs;
	this->collector_cb = new BUFFER(buffer_len); 
}

void Collector::body(){
	void* task = 0;
	size_t id_queue{0};
	if(this->sticky){move_to_context(this->get_id());}
	while(!(*wout_cbs)[id_queue]->safe_pop(&task)) continue;
	size_t eos_counter = (task == EOS) ? 1 : 0;
	while(eos_counter < (*wout_cbs).size()){ 
		size_t &t = (*((size_t*) task));
		//this->collector_cb->safe_push(&t); //safe_try
		id_queue = (++id_queue)%wout_cbs->size();
		while((*wout_cbs)[id_queue]->safe_pop(&task)) continue;
		eos_counter += (task == EOS) ? 1 : 0;
	};
	while(!this->collector_cb->safe_push(EOS)) continue;
	return;
}

void Collector::run(){
	this->thread = new std::thread(&Collector::body, this);
	return;
}

BUFFER* Collector::get_out_queue(){
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


Autonomic_Farm::Autonomic_Farm(size_t nw, std::function<size_t(size_t)> fun_body, size_t buffer_len, bool sticky, std::vector<size_t>* collection){ //max nw??
	//std::cout << "Machine Hardware Concurrency " << std::thread::hardware_concurrency() << std::endl;
	this->nw = nw;
	this->sticky = sticky;
	this->fun_body = fun_body;
	this->win_cbs = new std::vector<BUFFER*>();
	this->workers = new std::vector<Worker*>();
	this->wout_cbs = new std::vector<BUFFER*>();
	this->emitter = new Emitter(this->win_cbs, buffer_len, this->sticky, collection);
	this->collector = new Collector(this->wout_cbs, buffer_len, this->sticky);
	for(size_t i = 0; i < nw; i++)
		this->add_worker(buffer_len);
}

void Autonomic_Farm::run_and_wait(){
	this->emitter->run();
	for(size_t i = 0; i < nw; i++)
		(*this->workers)[i]->run();	
	this->collector->run();
	(*this->workers)[nw-1]->join();
	this->collector->join();
}


