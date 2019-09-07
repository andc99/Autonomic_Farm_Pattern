#include "context.h"
/////////////////////////////////////////////////////////////////////////
//
//	Context	
//
/////////////////////////////////////////////////////////////////////////

Context::Context(size_t context_id) : context_id(context_id){
	this->trace = new std::deque<Worker*>();
};

Context::~Context(){
	delete trace;
};

size_t Context::get_context_id(){
	return this->context_id;
}

size_t Context::get_n_threads(){
	return this->trace->size();
}

std::deque<Worker*>* Context::get_trace(){
	return this->trace;
}

void Context::move_in(Worker* w){
	this->trace->push_back(w);
	w->set_context(this->get_context_id());
	return;
}

Worker* Context::move_out(){
	Worker* w = this->trace->front(); 
	this->trace->pop_front();
	return w;
}


size_t Context::get_avg_ts(){
	size_t context_avg_ts = 0;
	for(auto worker : *(this->get_trace()))
		context_avg_ts+=worker->get_ts();
	return context_avg_ts/this->get_n_threads();
}
