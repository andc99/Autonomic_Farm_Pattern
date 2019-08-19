#include "buffer.h"

Buffer::Buffer(size_t size){
	this->size = size;
	this->d_mutex = new std::mutex();
	this->prev_push_time = std::chrono::high_resolution_clock::now();
	this->prev_pop_time = std::chrono::high_resolution_clock::now();
	return;
}

Buffer::~Buffer(){
	delete this->d_mutex;
	return;
}

void Buffer::update_mean_push_rate(){ //no lock se la ho già presa
	this->pushed_elements++;
	auto push_time = std::chrono::high_resolution_clock::now();
	long act_push_rate = std::chrono::duration_cast<std::chrono::microseconds>(push_time - this->prev_push_time).count();	
	this->mean_push_rate = this->mean_push_rate + (act_push_rate - this->mean_push_rate) / (this->pushed_elements);
	return;
}

void Buffer::update_mean_pop_rate(){
	this->popped_elements++;
	auto pop_time = std::chrono::high_resolution_clock::now();
	long act_pop_rate = std::chrono::duration_cast<std::chrono::microseconds>(pop_time - this->prev_pop_time).count();	
	this->mean_pop_rate = this->mean_pop_rate + (act_pop_rate - this->mean_push_rate) / (this->popped_elements);
	return;
}

bool Buffer::is_bottleneck(){
	std::lock_guard<std::mutex> lock(*d_mutex);
	float ro = static_cast<float>(this->mean_push_rate) / this->mean_pop_rate;
	//std::cout << "ro: " << ro << std::endl;
	return (ro > 1) ? true : false;
}
