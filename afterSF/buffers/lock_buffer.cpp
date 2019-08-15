#include "lock_buffer.h"

Lock_Buffer::Lock_Buffer(size_t size) : Buffer(size){
	this->p_condition = new std::condition_variable();
	this->c_condition = new std::condition_variable();
	return;
}

Lock_Buffer::~Lock_Buffer(){
	delete this->p_condition;
	delete this->c_condition;
	return;
}
