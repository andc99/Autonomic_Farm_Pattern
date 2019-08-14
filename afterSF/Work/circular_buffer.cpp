#include "circular_buffer.h"
#include <iostream>


Circular_Buffer::Circular_Buffer(size_t len) : size(len){
	this->circular_buffer = (void**) malloc(this->size*sizeof(void*));
	for(size_t i = 0; i < this->size; i++)
		this->circular_buffer[i] = NULL;
	this->d_mutex = new std::mutex();
	this->p_condition = new std::condition_variable();
	this->c_condition = new std::condition_variable();
	return;
}

Circular_Buffer::~Circular_Buffer(){
	delete [] this->circular_buffer;
	delete this->d_mutex;
	delete this->p_condition;
	delete this->c_condition;
	return;
}

bool Circular_Buffer::safe_push(void* const task){
	std::unique_lock<std::mutex> lock(*d_mutex);
	this->p_condition->wait(lock, [=]{return this->circular_buffer[this->p_write] == NULL;});
	this->circular_buffer[p_write] = task;
	this->p_write = (this->p_write == this->size - 1) ? 0 : this->p_write + 1;
	c_condition->notify_one();
	return true;
}

bool Circular_Buffer::try_safe_push(void* const task){
	if(this->d_mutex->try_lock()){
		this->circular_buffer[p_write] = task;
		this->p_write = (this->p_write == this->size - 1) ? 0 : this->p_write + 1;
		this->c_condition->notify_one();
		return true;
	}
	return false;
}

bool Circular_Buffer::safe_pop(void** task){
	std::unique_lock<std::mutex> lock(*d_mutex);
	this->c_condition->wait(lock, [=]{return this->circular_buffer[this->p_read] != NULL;});
	*task = this->circular_buffer[this->p_read];
	this->circular_buffer[this->p_read] = NULL;
	this->p_read = (this->p_read == this->size - 1) ? 0 : this->p_read + 1;
	this->p_condition->notify_one();
	return true;
}


void Circular_Buffer::safe_resize(size_t new_size){
	std::lock_guard<std::mutex> lock(*d_mutex);
	this->size = new_size;
	this->circular_buffer = (void**) realloc (this->circular_buffer, new_size*sizeof(void*));
	return;
}


size_t Circular_Buffer::safe_get_size(){
	std::lock_guard<std::mutex> lock(*d_mutex);
	return this->size;
}


