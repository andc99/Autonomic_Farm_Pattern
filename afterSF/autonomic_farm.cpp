#include "autonomic_farm.h"

ProcessingElement::ProcessingElement(bool sticky){
	this->sticky = sticky;
	static std::atomic<unsigned int> id{0};
	this->thread_id = id++; // (std::thread::hardware_concurrency);
	std::cout << "ID: " << this->thread_id << std::endl;
	if(this->sticky)	
		move_to_context(this->get_id());
	std::cout << "Machine Hardware Concurrency " << std::thread::hardware_concurrency << std::endl;
	return;
}

ProcessingElement::~ProcessingElement(){ //bypassabile per via dela join
	delete thread;
	return;
}

void ProcessingElement::join(){
	this->thread->join();
	return;
}

unsigned int ProcessingElement::get_id(){
	return this->thread_id;
}

unsigned int ProcessingElement::get_context(){
	return sched_getcpu();
}

unsigned int ProcessingElement::move_to_context(unsigned int id_context){
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(id_context%std::thread::hardware_concurrency(), &cpuset);
	int error = pthread_setaffinity_np(this->thread->native_handle(), sizeof(cpu_set_t), &cpuset);
	if (error != 0)
		std::cout << "Error calling pthread_setaffinity_np: " << error << "\n";
	std::cout << "ID: " << this->get_id() << " Context: " << this->get_context() << std::endl;
	return error;
}

/////////////////////////////////////////////////////////////////////////
//
//	Emitter
//
/////////////////////////////////////////////////////////////////////////
template <typename I>
Emitter<I>::Emitter(std::vector<Circular_Buffer*>* win_cbs, std::vector<I>* collection, bool sticky) : ProcessingElement(sticky){
	this->win_cbs = win_cbs;
	this->emitter_cb = new Circular_Buffer(50); // da cambiare!
	this->collection = collection;
}


template <typename I>
void Emitter<I>::body(){
	unsigned int id_queue{0};
	for(auto i = 0; i < (*collection).size(); i++){
		I &task = (*collection)[i];
		(*win_cbs)[id_queue]->safe_push(&task); //safe_try!!!!
		id_queue = (id_queue++)%win_cbs->size();
		std::cout << "ID_QUEUE EMITTER: " << id_queue << std::endl;
	}
	for(auto i = 0; i < win_cbs->size(); i++)
		(*win_cbs)[i]->safe_push(EOS); // qui non ci può andare la safe_try
	return;
}


template <typename I>
void Emitter<I>::run(){
	this->thread = new std::thread(&Emitter<I>::body, this);
	return;
}

template <typename I>
Circular_Buffer* Emitter<I>::get_in_queue(){
	return this->emitter_cb;
}


/////////////////////////////////////////////////////////////////////////
//
//	Worker	
//
/////////////////////////////////////////////////////////////////////////


template <typename I, typename O>
Worker<I,O>::Worker(std::function<I(O)> fun_body, bool sticky):ProcessingElement(sticky){
	this->fun_body = fun_body;
	this->win_cb = new Circular_Buffer(50); /////////yyp
	this->wout_cb = new Circular_Buffer(50); /////////yyp
};


template <typename I, typename O>
void Worker<I,O>::body(){
	void* task = 0;
	win_cb->safe_pop(&task);
	while( (I*) task != EOS){ //NON SICURO FUNZIONI
		std::cout << "entroo " <<std::endl;
		I &t = (*((I*) task));
		O res = fun_body(t);
		this->wout_cb->safe_push(&res); //safe_try
		win_cb->safe_pop(&task);	
	};
	this->wout_cb->safe_push(EOS);
	return;
}

template <typename I, typename O>
void Worker<I,O>::run(){
	this->thread = new std::thread(&Worker<I,O>::body_worker, this);
	return;
}


template <typename I, typename O>
Circular_Buffer* Worker<I,O>::get_in_queue(){
	return this->win_cb;
}
template <typename I, typename O>
Circular_Buffer* Worker<I,O>::get_out_queue(){
	return this->wout_cbs;
}








