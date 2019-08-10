#include "circular_buffer.h"
#include <thread>
#include <stdlib.h>
#include <iostream>
#include <math.h>

#define EOS -1

//gestire gli IFNOTDEF, ad esempio IFNOTDEF circular_buffer, define!

class ProcessingElement{
	protected:
		unsigned int thread_id;
		std::thread* thread;
		ProcessingElement();
		~ProcessingElement();
		virtual void run() = 0;

	public:
		void join();
		unsigned int get_id(); //calibrato rispetto al tipo di harware_concurrency che è unsigned int
		unsigned int get_context();
		unsigned int move_to_context(unsigned int id_context);
};


template<class I>
class Emitter : public ProcessingElement{

}
