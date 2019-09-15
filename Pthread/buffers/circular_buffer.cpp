#include "circular_buffer.h"


Circular_Buffer::Circular_Buffer(size_t size) : Lock_Buffer(size){
	this->circular_buffer = (void**) malloc(this->size*sizeof(void*));
	for(size_t i = 0; i < this->size; i++)
		this->circular_buffer[i] = NULL;
	return;
}

Circular_Buffer::~Circular_Buffer(){
	delete [] this->circular_buffer;
	return;
}

bool Circular_Buffer::safe_push(void* const task){
	std::unique_lock<std::mutex> lock(*d_mutex);
	this->p_condition->wait(lock, [=]{return this->circular_buffer[this->p_write] == NULL;});
	this->circular_buffer[p_write] = task;
	(this->p_write < this->size - 1) ? this->p_write++ : this->p_write = 0;
	this->c_condition->notify_one();
	return true;
}


bool Circular_Buffer::try_safe_push(void* const task){
	std::unique_lock<std::mutex> lock(*d_mutex, std::try_to_lock);
	if(lock.owns_lock()){
		this->circular_buffer[p_write] = task;
		(this->p_write < this->size - 1) ? this->p_write++ : this->p_write = 0;
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
	(this->p_read < this->size - 1) ? this->p_read++ : this->p_read = 0;
	this->p_condition->notify_one();
	return true;
}

bool Circular_Buffer::try_safe_pop(void **task){
	std::unique_lock<std::mutex> lock(*d_mutex, std::try_to_lock);
	if(lock.owns_lock()){
		*task = this->circular_buffer[this->p_read];
		this->circular_buffer[this->p_read] = NULL;
		(this->p_read < this->size - 1) ? this->p_read++ : this->p_read = 0;
		this->p_condition->notify_one();
		return true;
	}
	return false;
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


