#include "buffer.h"

Buffer::Buffer(size_t size){
	this->size = size;
	this->d_mutex = new std::mutex();
	return;
}

Buffer::~Buffer(){
	delete this->d_mutex;
	return;
}

void Buffer::update_mean_push_rate(){ //no lock -> già presa
	this->pushed_elements++;
	return;
}

void Buffer::update_mean_pop_rate(){
	this->popped_elements++;
	return;
}

bool Buffer::is_bottleneck(){
	std::lock_guard<std::mutex> lock(*d_mutex);
	float ro = static_cast<float>(this->pushed_elements) / this->popped_elements;
	std::cout << "ro: " << ro << std::endl;
	this->pushed_elements = this->popped_elements = 0; //reset to avoid overflow in long run
	return (ro > 1) ? true : false;
}
