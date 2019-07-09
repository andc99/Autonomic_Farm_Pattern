#include <chrono>
#include <math.h>
#include <stdlib.h>
#include <iostream>

#include <typeinfo>

using namespace std::chrono;

//std::chrono::high_resolution_clock::now()

int main(){
	high_resolution_clock::time_point start_time = high_resolution_clock::now();                
	high_resolution_clock::time_point end_time = high_resolution_clock::now();                
	std::cout << std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count() << std::endl;
	long variable =	std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
	std::cout << typeid(variable).name() << std::endl;
	std::cout << variable << std::endl;
}

