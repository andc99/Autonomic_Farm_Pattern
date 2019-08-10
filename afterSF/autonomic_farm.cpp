#include "autonomic_farm.h"

ProcessingElement::ProcessingElement(){
	static std::atomic<unsigned int> id{0};
	this->thread_id = id++; // (std::thread::hardware_concurrency);
	std::cout << "Machine Hardware Concurrency " << std::thread::hardware_concurrency << std::endl;
	return;
}

ProcessingElement::~ProcessingElement(){ //bypassabile per via dela join
	delete thread;
	return;
}

void ProcessingElement::join(){
	this->thread->join();
	return;
}

unsigned int ProcessingElement::get_id(){
	return this->thread_id;
}

unsigned int ProcessingElement::get_context(){
	return sched_getcpu();
}

unsigned int ProcessingElement::move_to_context(unsigned int id_context){
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(id_context%std::thread::hardware_concurrency(), &cpuset);
	int error = pthread_setaffinity_np(this->thread->native_handle(), sizeof(cpu_set_t), &cpuset);
	if (error != 0)
		std::cout << "Error calling pthread_setaffinity_np: " << error << "\n";
	std::cout << "ID: " << this->get_id() << " Context: " << this->get_context() << std::endl;
	return error;
}

/////////////////////////////////////////////////////////////////////////
//
//	Emitter
//
/////////////////////////////////////////////////////////////////////////
