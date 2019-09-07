#include "processing_element.h"

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


size_t ProcessingElement::get_ts(){
	std::lock_guard<std::mutex> lock(*stats_lock);
	return this->service_time;
}

void ProcessingElement::update_ts(size_t act_service_time){
	std::lock_guard<std::mutex> lock(*stats_lock);
	this->service_time = act_service_time;
	return;
}

void ProcessingElement::set_context(size_t context_id){
	std::lock_guard<std::mutex> lock(*context_id_lock);
	this->context_id = context_id;
	return;
}

int ProcessingElement::move_to_context(size_t id_context){
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(static_cast<unsigned int>(id_context)%std::thread::hardware_concurrency(), &cpuset); 
	int error = pthread_setaffinity_np(this->thread->native_handle(), sizeof(cpu_set_t), &cpuset);
	if (error != 0)
		std::cout << "Error calling pthread_setaffinity_np: " << error << "\n";
	else
		this->set_context(id_context); 
	return error;
}

