#include "circular_buffer.h"

void Circular_Buffer::init(){
	this->circular_buffer = (void**) malloc(this->size*sizeof(void*));
	this->d_mutex = new std::mutex();
	this->p_condition = new std::condition_variable();
	this->c_condition = new std::condition_variable();
	return;
}

Circular_Buffer::Circular_Buffer() : size(std::numeric_limits<unsigned int>::max()){
	init();
	return;
}

Circular_Buffer::Circular_Buffer(unsigned int len) : size(len){
	init();
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


void Circular_Buffer::safe_resize(unsigned int new_size){
	std::lock_guard<std::mutex> lock(*d_mutex);
	this->size = new_size;
	this->circular_buffer = (void**) realloc (this->circular_buffer, new_size*sizeof(void*));
	return;
}


unsigned int Circular_Buffer::safe_get_size(){
	std::lock_guard<std::mutex> lock(*d_mutex);
	return this->size;
}


