#include "safe_queue.h"

Safe_Queue::Safe_Queue(size_t size) : Lock_Buffer(size){
	this->queue = new std::queue<void*>();
	return;
}

Safe_Queue::~Safe_Queue(){
	delete this->queue;
	return;
}

bool Safe_Queue::safe_push(void* const task){
	std::unique_lock<std::mutex> lock(*d_mutex); 
	this->p_condition->wait(lock, [=]{return this->queue->size() < this->size;});   
	this->queue->push(task);
	c_condition->notify_one(); 
	return true;
}


bool Safe_Queue::try_safe_push(void* const task){
	std::unique_lock<std::mutex> lock(*d_mutex, std::try_to_lock);
	if(lock.owns_lock()){
		this->queue->push(task);
		c_condition->notify_one();
		return true;
	}
	return false;
}


bool Safe_Queue::safe_pop(void **task){
	std::unique_lock<std::mutex> lock(*d_mutex); 
	this->c_condition->wait(lock, [=]{return !this->queue->empty();});
	*task = this->queue->front();
	this->queue->pop();
	p_condition->notify_one();
	return true;
}

bool Safe_Queue::try_safe_pop(void **task){
	std::unique_lock<std::mutex> lock(*d_mutex, std::try_to_lock);
	if(lock.owns_lock()){
		*task = this->queue->front();
		this->queue->pop();
		p_condition->notify_one();
		return true;
	}
	return false;
}

void Safe_Queue::safe_resize(size_t new_size){
	std::lock_guard<std::mutex> lock(*d_mutex);
	this->size = new_size;
	return;
}

size_t Safe_Queue::safe_get_size(){
	std::lock_guard<std::mutex> lock(*d_mutex);
	return this->size;
}
