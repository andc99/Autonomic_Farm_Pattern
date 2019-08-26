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
	this->context_id = id_context; //se errore non assegnare. Cambiare nome variabile
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

Emitter::Emitter(std::vector<Buffer*>* win_cbs, size_t buffer_len, std::vector<ssize_t>* collection) : ProcessingElement(){
	this->win_cbs = win_cbs;
	this->emitter_cb = new BUFFER(buffer_len); 
	this->collection = collection;
}

void Emitter::body(){
	size_t id_queue{0};
	long act_service_time = 0;
	std::chrono::high_resolution_clock::time_point start_time, end_time;
	move_to_context(get_context());
	for(size_t i = 0; i < (*collection).size(); i++){
		start_time = std::chrono::high_resolution_clock::now();
		ssize_t &task = (*collection)[i];
		(*win_cbs)[id_queue]->safe_push(&task); //safe_try!!!!
		id_queue = (++id_queue)%win_cbs->size();	
		end_time = std::chrono::high_resolution_clock::now();
		act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
		this->update_stats(act_service_time);
	}
	for(auto win_cb : *(this->win_cbs))
		win_cb->safe_push(EOS); // qui non ci può andare la safe_try
	return;
}

void Emitter::run(){
	this->thread = new std::thread(&Emitter::body, this);
	return;
}

Buffer* Emitter::get_in_queue(){
	return this->emitter_cb;
}


/////////////////////////////////////////////////////////////////////////
//
//	Worker	
//
/////////////////////////////////////////////////////////////////////////


Worker::Worker(std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len):ProcessingElement(){
	this->fun_body = fun_body;
	this->win_cb = new BUFFER(buffer_len); 
	this->wout_cb = new BUFFER(buffer_len); 
};

void Worker::body(){
	void* task = 0;
	long act_service_time = 0;
	std::chrono::high_resolution_clock::time_point start_time, end_time;
	this->move_to_context(get_context());
	this->win_cb->safe_pop(&task);
	while( task != EOS){ 
		start_time = std::chrono::high_resolution_clock::now();
		ssize_t &t = (*((ssize_t*) task));
		t = (fun_body(t));
		this->wout_cb->safe_push(&t); //safe_try
		this->win_cb->safe_pop(&task);	
		end_time = std::chrono::high_resolution_clock::now();
		if (this->get_context() != sched_getcpu())
			move_to_context(this->get_context());
		act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
		this->update_stats(act_service_time);
	};
	this->wout_cb->safe_push(EOS);
	return;
}

void Worker::run(){
	this->thread = new std::thread(&Worker::body, this);
	return;
}


Buffer* Worker::get_in_queue(){
	return this->win_cb;
}

Buffer* Worker::get_out_queue(){
	return this->wout_cb;
}

/////////////////////////////////////////////////////////////////////////
//
//	Collector	
//
/////////////////////////////////////////////////////////////////////////

Collector::Collector(std::vector<Buffer*>* wout_cbs, size_t buffer_len):ProcessingElement(){
	this->wout_cbs = wout_cbs;
	this->collector_cb = new BUFFER(buffer_len); 
}

void Collector::body(){
	void* task = 0;
	size_t id_queue{0};
	long act_service_time = 0;
	std::chrono::high_resolution_clock::time_point start_time, end_time;
	move_to_context(get_context());
	(*wout_cbs)[id_queue]->safe_pop(&task);
	size_t eos_counter = (task == EOS) ? 1 : 0;
	while(eos_counter < (*wout_cbs).size()){ 
		start_time = std::chrono::high_resolution_clock::now();
		ssize_t &t = (*((ssize_t*) task));
		//this->collector_cb->safe_push(&t); //safe_try
		id_queue = (++id_queue)%wout_cbs->size();
		(*wout_cbs)[id_queue]->safe_pop(&task);
		eos_counter += (task == EOS) ? 1 : 0;
		end_time = std::chrono::high_resolution_clock::now();
		act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
		this->update_stats(act_service_time);
	};
	this->collector_cb->safe_push(EOS);
	return;
}

void Collector::run(){
	this->thread = new std::thread(&Collector::body, this);
	return;
}

Buffer* Collector::get_out_queue(){
	return this->collector_cb;
}

/////////////////////////////////////////////////////////////////////////
//
//	Autonomic Farm	
//
/////////////////////////////////////////////////////////////////////////

Worker* Autonomic_Farm::add_worker(size_t buffer_len){ //c'è da runnarlo poi ehhhh
	Worker* worker = new Worker(this->fun_body, buffer_len);
	this->win_cbs->push_back(worker->get_in_queue());
	this->wout_cbs->push_back(worker->get_out_queue());
	(*this->workers).push_back(worker);
	return worker;
}

long Autonomic_Farm::get_service_time_farm(){
	long mean_service_time_workers = 0;
	for(auto worker : (*this->workers))
		mean_service_time_workers+=worker->get_mean_service_time();
	mean_service_time_workers/=this->max_nw;

	return std::max({this->emitter->get_mean_service_time(),
			this->collector->get_mean_service_time(),
			mean_service_time_workers/static_cast<long>(this->nw)});//questo è nw perchè sono quelli effittivi
			//castato max_nw a long... fare check sui tipi eh
}


//passare come parametro questoooo
std::function<Buffer*()> next_buffer(size_t max_nw, std::vector<Buffer*>* buffers){
	size_t id_queue{0};
	return [&id_queue, max_nw, buffers]() mutable {
		id_queue = (id_queue < max_nw) ? ++id_queue : id_queue = 0;	
		return (*buffers)[id_queue];
	};
}


Autonomic_Farm::Autonomic_Farm(long ts_goal, unsigned int nw, unsigned int max_nw, std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len, std::vector<ssize_t>* collection) : ts_goal(ts_goal), fun_body(fun_body){ 
	//fare il check che max_nw non può essere maggiore dell'hardware concurrency
	if(nw > max_nw){
		std::cout << "Error nw > max_nw" << std::endl;
		return;
	}
	unsigned int n_contexts = std::thread::hardware_concurrency();
	this->max_nw = max_nw;
	this->nw = (nw < n_contexts) ? nw : n_contexts; //NO -> //caso in cui si inserisca un nw maggiore rispetto alla concurrency massima effettiva e metto i rimanenti fuori (nw - hardware_concurrency su max_nw)
	std::cout << this->nw << std::endl;
	std::cout << this->max_nw << std::endl;
	this->stop = new std::atomic<bool>(false);
	this->win_cbs = new std::vector<Buffer*>();
	this->workers = new std::vector<Worker*>();
	this->wout_cbs = new std::vector<Buffer*>();
	
	std::function<Buffer*()> emitter_next_buffer = next_buffer(this->max_nw, this->win_cbs);
	std::function<Buffer*()> collector_next_buffer = next_buffer(this->max_nw, this->wout_cbs);
	this->emitter = new Emitter(this->win_cbs, buffer_len, collection);
	this->collector = new Collector(this->wout_cbs, buffer_len);

	for(auto i = 0; i < this->max_nw; i++) //vanno possibilmente su core distinti tra loro
		this->add_worker(buffer_len);

	this->manager = new Manager(this, this->ts_goal,
			this->stop, this->emitter,
			this->collector, (std::vector<ProcessingElement*>*) this->workers,
			this->nw, this->max_nw, n_contexts);
}

void Autonomic_Farm::run_and_wait(){
	this->emitter->run();
	for(auto worker : (*this->workers))
		worker->run();	
	this->collector->run();	
	this->manager->run();
	this->collector->join();
	*(this->stop) = true;
	this->manager->join();
	std::cout << "Emitter: " << this->emitter->get_mean_service_time() << " - " << this->emitter->get_variance_service_time() << std::endl;
	for(size_t i = 0; i < max_nw; i++)
		std::cout << "Worker " << (*this->workers)[i]->get_id() << ": " << (*this->workers)[i]->get_mean_service_time() << " - " << (*this->workers)[i]->get_variance_service_time() << std::endl;
	std::cout << "Collector: " << this->collector->get_mean_service_time() << " - " << this->collector->get_variance_service_time() << std::endl;
}

//quando faccio la set_context devo anche vedere se su quella deque c'è già un elemento, perchè in quel caso, sì aumento ma sono in hyperthreading
Manager::Manager(Autonomic_Farm* autonomic_farm, long ts_goal, std::atomic<bool>* stop, ProcessingElement* emitter, ProcessingElement* collector,
		std::vector<ProcessingElement*>* workers,
		unsigned int nw, unsigned int max_nw, unsigned int n_contexts) : autonomic_farm(autonomic_farm), ts_goal(ts_goal), stop(stop), nw(nw), max_nw(max_nw), ProcessingElement(){
	//il throughput per ocllector ed emitter, usiamo la varianza perchè Prendono il task e lo mettono da un'altra parte
//voglio ncontexts e non max_nw, quelli sono già stati fissati, perchè altrimenti taglierei fuori dei contesti sui quali potrei spostarmi nel caso di rallentamenti
	unsigned int i = 0;
	for(auto context_id = 0; context_id < n_contexts; context_id++) 
		this->idle.push_back(context_id);
	//this->wake_worker(emitter);
	//this->wake_worker(collector); // deve andare sopra l'emitter
	//this->wake_worker(this); // deve andare sopra l'emitter
	while(i < this->max_nw){
		(i < nw) ? this->wake_worker((*workers)[i]) : this->idle_worker((*workers)[i]);
		std::cout << "ID: " << (*workers)[i]->get_id() << " --- " << (*workers)[i]->get_context() << std::endl;
		i++;
	}
	std::cout << "ACTIVE" << std::endl;
	for(auto a : this->wake)
		std::cout << a << std::endl;
	std::cout << "\nIDLE" << std::endl;
	for(auto i : this->idle)
		std::cout << i << std::endl;
	return;
}

void Manager::run(){
	this->thread = new std::thread(&Manager::body, this);
	return;
}

void Manager::body(){
	std::chrono::high_resolution_clock::time_point start_time, end_time;
	long act_service_time;
	while(!(*this->stop)){
		int rest = rand() % 2000 + 200;
		std::this_thread::sleep_for(std::chrono::milliseconds(rest));
		start_time = std::chrono::high_resolution_clock::now();
		std::cout << "***************" << std::endl;
		for(auto const& [key,val] : this->threads_trace){
			for(auto pe : val)
				std::cout << "ID: " << pe->get_id() << " --- " << pe->get_context() << std::endl;
		}
		/*		if(pe->get_in_queue()->is_bottleneck())
						this->increase_degree();
		}*/
		//deve anche decrementare
		long act_ts = this->autonomic_farm->get_service_time_farm();
		std::cout << " >> " << act_ts << std::endl;
		if( act_ts > this->ts_goal){	
			this->increase_degree();
		}
		end_time = std::chrono::high_resolution_clock::now();
		act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
		this->update_stats(act_service_time);
	};
	return;

}



//
void Manager::wake_worker(ProcessingElement* pe){
	size_t x = this->idle.front();
	this->idle.pop_front(); //gestire il caso in cui nw sia maggiore del numero dei contesti (la coda salta male, non ci trova niente)
	pe->set_context(x);
	this->threads_trace[x].push_front(pe);
	this->wake.push_front(x); //aiaiai
	return;
}

//mettere quello sottratto in fondo
void Manager::increase_degree(){
	size_t z = this->wake.back(); //guardo l'elemento in coda
	std::cout << " 1 " <<  (this->threads_trace[z].size()) << std::endl;
	std::cout << " 2 " << (this->idle.size()) << std::endl;
	if(this->threads_trace[z].size()>1 && this->idle.size()>0){ //la size è > 1? sì allora posso svegliare, per questo motivo però dovrei escludere collector ed emitter da questa cosa ma li accedo direttamente con il get_context
		this->wake.pop_back();
		ProcessingElement* pe = this->threads_trace[z].front();
		this->threads_trace[z].pop_front();
		this->wake.push_front(z);
		wake_worker(pe);
		nw++;
	}
	std::cout << " >> " << nw << std::endl;
	return;
}

//Assunzione: se voglio svegliarne uno, so che in coda alla wake ci sono
//i core con un numero maggiore di di thread sopra, quindi prendo da quello l'id
//e dall'id vado nella trace, faccio una pop e lo metto su un core libero e aggionro wake 
//SE E SOLO SE IDLE è != da 0!!!!
//idle non mi dice quanti worker dormono ma quanti contesti ho a disposizione!!
//La wake queue è ordinata. in fondo i core più appesantiti e i testa quelli più leggeri
void Manager::idle_worker(ProcessingElement* pe){
	size_t x = this->wake.front(); //quelli con meno pesantezza li metto dietro
	this->wake.pop_front();
	pe->set_context(x); //lo sovrappongo ad uno già presente
	this->threads_trace[x].push_front(pe);
	this->wake.push_back(x); //li ruoto
	return;
}

void Manager::decrease_degree(){
	size_t z = this->wake.front();
	if(this->threads_trace[z].size() > 0 && this->wake.size() >0){
		this->wake.pop_front();
		while(!this->threads_trace[z].empty()){
			ProcessingElement* pe = this->threads_trace[z].front();
			this->threads_trace[z].pop_front();
			this->idle_worker(pe);
			nw--;
		}
		this->idle.push_back(z); //ho liberato un core totalmente!
	}
	return;
}


