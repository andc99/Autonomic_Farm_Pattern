#include "circular_buffer.h"
#include <thread>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <vector>
#include <functional>

#define EOS (void*)-1

//gestire gli IFNOTDEF, ad esempio IFNOTDEF circular_buffer, define!

class ProcessingElement{
	protected:
		bool sticky;
		unsigned int thread_id;
		std::thread* thread;
		ProcessingElement(bool sticky);
		~ProcessingElement();
		virtual void body() = 0;
		virtual void run() = 0;

	public:
		void join();
		unsigned int get_id(); //calibrato rispetto al tipo di harware_concurrency che è unsigned int
		unsigned int get_context();
		unsigned int move_to_context(unsigned int id_context);
};


template<class I>
class Emitter : public ProcessingElement{
	private:
		std::vector<Circular_Buffer*>* win_cbs; //input queues to workers
		Circular_Buffer* emitter_cb; //potrebbe non essere necessario, dovrebbero esserfe concatenabili (?)
		std::vector<I>* collection;

	public:
		Emitter(std::vector<Circular_Buffer*>* win_cbs, std::vector<I>* collection, bool sticky);//Super??
		void body();
		void run();
		Circular_Buffer* get_in_queue();
};

template<class I, class O>
class Worker : public ProcessingElement{
	private:
		std::function<I(O)> fun_body;
		Circular_Buffer* win_cb;
		Circular_Buffer* wout_cb;

	public: 
		Worker(std::function<I(O)> fun_body, bool sticky);
		void body();
		void run();
		Circular_Buffer* get_in_queue();
		Circular_Buffer* get_out_queue();

};
