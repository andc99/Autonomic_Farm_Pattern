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

