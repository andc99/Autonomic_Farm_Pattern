#include "autonomic_farm.h"


/////////////////////////////////////////////////////////////////////////
//
//	ProcessingElement	
//
/////////////////////////////////////////////////////////////////////////


ProcessingElement::ProcessingElement(){
	static std::atomic<size_t> id{0};
	this->thread_id = id++; 
	this->context_id_lock = new std::mutex();
	this->stats_lock = new std::mutex();
}

ProcessingElement::~ProcessingElement(){ //bypassabile per via dela join
	delete thread;
	delete this->context_id_lock;
	delete this->stats_lock;
	return;
}

void ProcessingElement::join(){
	this->thread->join();
	return;
}

size_t ProcessingElement::get_id(){ 
	return this->thread_id;
}


size_t ProcessingElement::get_context(){
	std::lock_guard<std::mutex> lock(*context_id_lock);
	return this->context_id;
}


void ProcessingElement::set_context(size_t context_id){
	std::lock_guard<std::mutex> lock(*context_id_lock);
	this->context_id = context_id;
	return;
}

ssize_t ProcessingElement::move_to_context(size_t id_context){
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(static_cast<int>(id_context)%static_cast<int>(std::thread::hardware_concurrency()), &cpuset); //module %
	ssize_t error = pthread_setaffinity_np(this->thread->native_handle(), sizeof(cpu_set_t), &cpuset);
	if (error != 0)
		std::cout << "Error calling pthread_setaffinity_np: " << error << "\n";
	this->set_context(id_context); //se errore non assegnare. Cambiare nome variabile
	return error;
}

long ProcessingElement::get_mean_service_time(){
	std::lock_guard<std::mutex> lock(*stats_lock);
	return this->mean_service_time;
}

long ProcessingElement::get_variance_service_time(){
	std::lock_guard<std::mutex> lock(*stats_lock);
	return this->variance_service_time;
}

void ProcessingElement::update_stats(long act_service_time){
	std::lock_guard<std::mutex> lock(*stats_lock);
	size_t pred_mean_service_time = this->mean_service_time;
	this->processed_elements++;
	this->update_mean_service_time(act_service_time);
	this->update_variance_service_time(act_service_time, pred_mean_service_time);
	return;
}

long ProcessingElement::update_mean_service_time(long act_service_time){
	this->mean_service_time = this->mean_service_time + (act_service_time - this->mean_service_time)/(this->processed_elements);
	return this->mean_service_time;
}

long ProcessingElement::update_variance_service_time(long act_service_time, long pred_mean_service_time){
	long sn = this->variance_service_time + (act_service_time - pred_mean_service_time)*(act_service_time - this->mean_service_time);
	this->variance_service_time = sqrt((sn/this->processed_elements));
	return this->variance_service_time;
}

/////////////////////////////////////////////////////////////////////////
//
//	Emitter
//
/////////////////////////////////////////////////////////////////////////



Emitter::Emitter(std::vector<BUFFER*>* win_bfs, size_t buffer_len, std::vector<ssize_t>* collection) : n_buffers(win_bfs->size()), ProcessingElement(){

	this->next_push = Emitter::rotate_push(win_bfs);
	this->emitter_buffer = new BUFFER(buffer_len); 
	this->collection = collection;
}

void Emitter::body(){
	long act_service_time = 0;
	std::chrono::high_resolution_clock::time_point start_time, end_time;
	for(auto i = 0; i < (*collection).size(); i++){
		start_time = std::chrono::high_resolution_clock::now();
		ssize_t &task = (*collection)[i];
		this->next_push(&task);
		if (this->get_context() != sched_getcpu())
			move_to_context(get_context());
		end_time = std::chrono::high_resolution_clock::now();
		act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
		this->update_stats(act_service_time);
	}
	for(auto i = 0; i < this->n_buffers; i++){
		this->next_push(EOS);
	}
	return;
}

void Emitter::run(){
	this->thread = new std::thread(&Emitter::body, this);
	return;
}

//in case stream
BUFFER* Emitter::get_in_buffer(){
	return this->emitter_buffer;
}


/////////////////////////////////////////////////////////////////////////
//
//	Worker	
//
/////////////////////////////////////////////////////////////////////////


Worker::Worker(std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len):ProcessingElement(){
	this->fun_body = fun_body;
	this->win_bf = new BUFFER(buffer_len); 
	this->wout_bf = new BUFFER(buffer_len); 
};

void Worker::body(){
	void* task = 0;
	long act_service_time = 0;
	std::chrono::high_resolution_clock::time_point start_time, end_time;
	this->win_bf->safe_pop(&task);
	while( task != EOS){ 
		start_time = std::chrono::high_resolution_clock::now();
		ssize_t &t = (*((ssize_t*) task));
		t = (fun_body(t));
		this->wout_bf->safe_push(&t); 
		this->win_bf->safe_pop(&task);	
		if (this->get_context() != sched_getcpu())
			move_to_context(this->get_context());
		end_time = std::chrono::high_resolution_clock::now();
		act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
		this->update_stats(act_service_time);
	};
	this->wout_bf->safe_push(EOS);
	return;
}

void Worker::run(){
	this->thread = new std::thread(&Worker::body, this);
	return;
}


BUFFER* Worker::get_in_buffer(){
	return this->win_bf;
}

BUFFER* Worker::get_out_buffer(){
	return this->wout_bf;
}

/////////////////////////////////////////////////////////////////////////
//
//	Collector	
//
/////////////////////////////////////////////////////////////////////////

Collector::Collector(std::vector<BUFFER*>* wout_bfs, size_t buffer_len) : n_buffers(wout_bfs->size()), ProcessingElement(){
	this->next_pop = Collector::rotate_pop(wout_bfs);
	this->collector_buffer = new BUFFER(buffer_len); 
}

void Collector::body(){
	void* task = 0;
	long act_service_time = 0;
	size_t eos_counter = 0;
	std::chrono::high_resolution_clock::time_point start_time, end_time;
	while(eos_counter < this->n_buffers){ 
		start_time = std::chrono::high_resolution_clock::now();
		this->next_pop(&task);
		eos_counter += (task == EOS) ? 1 : 0;
		ssize_t &t = (*((ssize_t*) task));
		this->collector_buffer->safe_push(&t); 
		if (this->get_context() != sched_getcpu())
			move_to_context(get_context());
		end_time = std::chrono::high_resolution_clock::now();
		act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
		this->update_stats(act_service_time);
	};
	this->collector_buffer->safe_push(EOS);
	return;
}

void Collector::run(){
	this->thread = new std::thread(&Collector::body, this);
	return;
}

BUFFER* Collector::get_out_buffer(){
	return this->collector_buffer;
}


/////////////////////////////////////////////////////////////////////////
//
//	Context	
//
/////////////////////////////////////////////////////////////////////////

Context::Context(unsigned id_context) : id_context(id_context){};

unsigned int Context::get_id_context(){
	return this->id_context;
}

unsigned int Context::get_n_threads(){
	return this->trace.size();
}

std::deque<ProcessingElement*> Context::get_trace(){
	return this->trace;
}

void Context::move_in(ProcessingElement* pe){
	this->trace.push_back(pe);
	pe->set_context(this->id_context);
	return;
}

ProcessingElement* Context::move_out(){
	ProcessingElement* pe = this->trace.front(); 
	this->trace.pop_front();
	return pe;
}

/////////////////////////////////////////////////////////////////////////
//
//	Manager	
//
/////////////////////////////////////////////////////////////////////////


//se nw è maggiore del numero dei contesti? --> gestire
void Manager::wake_workers(unsigned int n){
	n = (n < this->max_nw-this->nw) ? n : max_nw-this->nw;
	n = (n < this->idle.size()) ? n : this->idle.size();
	ProcessingElement* pe = NULL; 
	Context *act_context, *idle_context;
	for(auto i = 0; i < n; i++){
		idle_context = this->idle.front();
		this->idle.pop_front();
		act_context = this->active_contexts.front();
		this->active_contexts.pop_front();
		pe = act_context->move_out();
		idle_context->move_in(pe);
		this->active_contexts.push_front(idle_context);	
		this->active_contexts.push_back(act_context);
		this->nw++;
	}	
	this->redistribute();
	return;
}

//rimuovere del tutto il contesto e rimettero su idle se vuoto
//se non ci sono abbastanza worker da risvegliare? li prendo dal contesto successivo
void Manager::idle_workers(unsigned int n){ // qui non dovrei aver bisogno di check sulla n
	n = (n < this->active_contexts.size()) ? n : this->active_contexts.size()-1; //ne lascio uno sveglio
	ProcessingElement* pe = NULL;
	Context *act_context, *idle_context;
	for(auto i = 0; i < n; i++){
		idle_context = this->active_contexts.back();
		this->active_contexts.pop_back();
		while(idle_context->get_n_threads() > 0){ //più pesanti sono sul front //da vedere altrimenti c'è da mettere che parte dal fondo
			pe = idle_context->move_out();
			act_context = this->active_contexts.front();
			act_context->move_in(pe);
			this->active_contexts.pop_front();
			this->active_contexts.push_back(act_context); //per far ruotare su chi li distribuisco
			
		}
		this->idle.push_back(idle_context);	
		this->nw--;
	}
	this->redistribute();
	return;
}


long Manager::get_service_time_farm(){
	long mean_service_time_workers = 0;
	for(auto context : this->active_contexts) //a sto punto ce li ho divisi per contesto ehh ottimizzazioni?
		for(auto worker : context->get_trace())
			mean_service_time_workers+=worker->get_mean_service_time();
	mean_service_time_workers/=this->max_nw;
	return std::max({this->emitter->get_mean_service_time(),
			this->collector->get_mean_service_time(),
			mean_service_time_workers/static_cast<long>(this->nw)});//questo è nw perchè sono quelli effittivi
			//castato max_nw a long... fare check sui tipi eh
}



//il throughput per ocllector ed emitter, usiamo la varianza perchè Prendono il task e lo mettono da un'altra parte
//voglio ncontexts e non max_nw, quelli sono già stati fissati, perchè altrimenti taglierei fuori dei contesti sui quali potrei spostarmi nel caso di rallentamenti
Manager::Manager(long ts_goal, std::atomic<bool>* stop, Emitter* emitter, Collector* collector,
		std::vector<ProcessingElement*>* workers,
		unsigned int nw, unsigned int max_nw, unsigned int n_contexts) : ts_goal(ts_goal), stop(stop), nw(nw), max_nw(max_nw), ProcessingElement(){
	this->emitter = emitter;
	this->collector = collector;
	for(auto id_context = 0; id_context < n_contexts; id_context++)
		this->idle.push_back(new Context(id_context));
	//this->wake_worker(collector); // deve andare sopra l'emitter
	//this->wake_worker(this); // deve andare sopra l'emitter
	Context* context;
	for(auto i = 0; i < nw; i++){
		context = this->idle.front();
		this->idle.pop_front();
		context->move_in((*workers)[i]);
		this->active_contexts.push_back(context);
	}
	for(auto i = nw; i < max_nw; i++) 
		this->active_contexts[i%nw]->move_in((*workers)[i]);
	return;
}

//devo poi aggiornare gli active e gli idle!
void Manager::transfer_threads_to_core(Context* from, Context* to){
	ProcessingElement* pe = NULL;
	while(!from->get_trace().empty()){
		pe = from->move_out();
		to->move_in(pe);
	}
	return;
}

void Manager::redistribute(){
	unsigned int r = this->max_nw / this->nw;
	unsigned int m = this->max_nw % this->nw;
	for(auto context : this->active_contexts)
		this->resize(context, r);
	ProcessingElement* pe = NULL;
	for(auto context : this->active_contexts){
		while(context->get_n_threads() < r){
			pe = this->pes_queue.front();
			this->pes_queue.pop_front();
			context->move_in(pe);
		}
	}
	Context* context;
	while(!this->pes_queue.empty()){
		pe = this->pes_queue.front();
		this->pes_queue.pop_front();
		this->active_contexts.front()->move_in(pe);
		context = this->active_contexts.front();
		this->active_contexts.pop_front();
		this->active_contexts.push_back(context);
	}
	return;
}

void Manager::resize(Context* context, unsigned int size){
	while(context->get_n_threads() > size){
		ProcessingElement* pe = context->move_out();
		this->pes_queue.push_front(pe);
	}
	return;	
}


//c'è un caso dove smette di scalare: quello in cui in testa ha un elemento da 1 solo thread dentro:w
//
void Manager::body(){
	std::chrono::high_resolution_clock::time_point start_time, end_time;
	long act_service_time;
	while(!(*this->stop)){
		int rest =  rand() % 1200 + 200;
		std::this_thread::sleep_for(std::chrono::milliseconds(rest));
		start_time = std::chrono::high_resolution_clock::now();
				//for(auto const& [key,val] : this->threads_trace){
		//	for(auto pe : val)
		//		std::cout << "ID: " << pe->get_id() << " --- " << pe->get_context() << std::endl;
		//}
		/*		if(pe->get_in_queue()->is_bottleneck())
						this->increase_degree();
		}*/
		//deve anche decrementare
		std::cout << "\n***************" << std::endl;
		std::cout << " ACTIVE " << std::endl;
		for(auto i = 0; i < this->active_contexts.size(); i++)
			std::cout << this->active_contexts[i]->get_id_context() << " - " << this->active_contexts[i]->get_n_threads() << std::endl;
		std::cout << " IDLE " << std::endl;
		for(auto i = 0; i < this->idle.size(); i++)
			std::cout << this->idle[i]->get_id_context() << " - " << this->idle[i]->get_n_threads() << std::endl;
		std::cout << " -------- " << std::endl;

		for(auto const& context : active_contexts){
			std::deque<ProcessingElement*> trace = context->get_trace();
			for(auto const& pe : trace){
				std::cout << "Worker " << pe->get_id() << ": " << pe->get_mean_service_time() << " - " << pe->get_variance_service_time() << std::endl;
			}
		}
		long act_ts = this->get_service_time_farm();
		std::cout << " sss " << act_ts << std::endl;
		if( act_ts > this->ts_goal){	
			unsigned int n = act_ts/ts_goal; //devono esserci totali n! oppure devono essere aggiunti n?
			std::cout << " increased " << n << std::endl;
			this->wake_workers(n);
		}else if(act_ts < this->ts_goal*95/100){
			unsigned int n = this->ts_goal*95/100/act_ts; //devono esserci totali n! oppure devono essere aggiunti n?
			this->idle_workers(n);
		}else{
			std::cout << "stable" << std::endl;
		}
		end_time = std::chrono::high_resolution_clock::now();
		act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
		this->update_stats(act_service_time);
		std::cout << " >> " << nw << std::endl;
	};
	return;

}

void Manager::run(){
	this->thread = new std::thread(&Manager::body, this);
	return;
}

/////////////////////////////////////////////////////////////////////////
//
//	Autonomic Farm	
//
/////////////////////////////////////////////////////////////////////////
//passare come parametro questoooo


Worker* Autonomic_Farm::add_worker(std::vector<BUFFER*>* win_bfs, std::vector<BUFFER*>* wout_bfs, size_t buffer_len){ 
	Worker* worker = new Worker(this->fun_body, buffer_len);
	win_bfs->push_back(worker->get_in_buffer());
	wout_bfs->push_back(worker->get_out_buffer());
	(*this->workers).push_back(worker);
	return worker;
}

Autonomic_Farm::Autonomic_Farm(long ts_goal, unsigned int nw, unsigned int max_nw, std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len, std::vector<ssize_t>* collection) : ts_goal(ts_goal), fun_body(fun_body){ 
	if(nw > max_nw){
		std::cout << "Error nw > max_nw" << std::endl;
		return;
	}
	if(ts_goal == 0){
		std::cout << "ts_goal is 0" << std::endl;
		return;
	}
	unsigned int n_contexts = std::thread::hardware_concurrency();
	nw = (nw < n_contexts) ? nw : n_contexts; 
	this->max_nw = (max_nw < n_contexts) ? max_nw : n_contexts;
	std::cout << nw << std::endl;
	std::cout << this->max_nw << std::endl;
	this->stop = new std::atomic<bool>(false);
	std::vector<BUFFER*>* win_bfs = new std::vector<BUFFER*>();
	this->workers = new std::vector<Worker*>();
	std::vector<BUFFER*>* wout_bfs = new std::vector<BUFFER*>();	

	for(auto i = 0; i < this->max_nw; i++) 
		this->add_worker(win_bfs, wout_bfs, buffer_len);

	this->emitter = new Emitter(win_bfs, buffer_len, collection);
	this->collector = new Collector(wout_bfs, buffer_len);

	this->manager = new Manager(this->ts_goal,
			this->stop, this->emitter,
			this->collector, (std::vector<ProcessingElement*>*) this->workers,
			nw, this->max_nw, n_contexts);
}

void Autonomic_Farm::run(){
	this->emitter->run();
	for(auto worker : (*this->workers))
		worker->run();	
	this->manager->run();
	this->collector->run();	
	return;
}

void Autonomic_Farm::join(){
	for(auto worker : (*this->workers))
		worker->join();	
	this->collector->join();
	*(this->stop) = true;
	this->manager->join();
	return;
}

/* altrimenti devo gestire il caso togliendo le push del collector
void Autonomic_Farm::run_and_wait(){
	this->run();
	this->join();
}
*/

size_t Autonomic_Farm::pop_outputs(){
	void* task = 0;
	this->collector->get_out_buffer()->safe_pop(&task);
	size_t &t = (*((size_t*) task));
	return task == EOS ? -1 : t;
}
