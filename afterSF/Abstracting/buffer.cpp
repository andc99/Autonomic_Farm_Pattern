#include "buffer.h"

Buffer::Buffer(size_t size){
	this->size = size;
	this->d_mutex = new std::mutex();
	this->p_condition = new std::condition_variable();
	this->c_condition = new std::condition_variable();
	return;
}

Buffer::~Buffer(){
	delete this->d_mutex;
	delete this->p_condition;
	delete this->c_condition;
	return;
}
