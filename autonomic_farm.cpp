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

ProcessingElement::~ProcessingElement(){ 
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

int ProcessingElement::move_to_context(size_t id_context){
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(id_context%std::thread::hardware_concurrency(), &cpuset); 
	int error = pthread_setaffinity_np(this->thread->native_handle(), sizeof(cpu_set_t), &cpuset);
	if (error != 0)
		std::cout << "Error calling pthread_setaffinity_np: " << error << "\n";
	else
		this->set_context(id_context); 
	return error;
}

long ProcessingElement::get_ts(){
	std::lock_guard<std::mutex> lock(*stats_lock);
	return this->service_time;
}

void ProcessingElement::update_ts(long act_service_time){
	std::lock_guard<std::mutex> lock(*stats_lock);
	this->service_time = act_service_time;
	return;
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
	for(auto i = 0; i < (*collection).size(); i++){
		this->start_time = std::chrono::high_resolution_clock::now();
		ssize_t &task = (*collection)[i];
		this->next_push(&task);
		if (this->get_context() != sched_getcpu())
			move_to_context(get_context());
		this->end_time = std::chrono::high_resolution_clock::now();
		act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(this->end_time - this->start_time).count();
		this->update_ts((act_service_time <= 0) ? 1 : act_service_time);
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
	this->win_bf->safe_pop(&task);
	std::chrono::high_resolution_clock::time_point time_ts, time_te;
	while( task != EOS){ 
		this->start_time = std::chrono::high_resolution_clock::now();
		ssize_t &t = (*((ssize_t*) task));
		t = (fun_body(t));
		this->wout_bf->safe_push(&t); 
		this->win_bf->safe_pop(&task);	
		if (this->get_context() != sched_getcpu())
			move_to_context(this->get_context());
		this->end_time = std::chrono::high_resolution_clock::now();
		act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(this->end_time - this->start_time).count();
		this->update_ts(act_service_time);
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
	while(eos_counter < this->n_buffers){ 
		this->start_time = std::chrono::high_resolution_clock::now();
		this->next_pop(&task);
		eos_counter += (task == EOS) ? 1 : 0;
		ssize_t &t = (*((ssize_t*) task));
		this->collector_buffer->safe_push(&t); 
		if (this->get_context() != sched_getcpu())
			move_to_context(get_context());
		this->end_time = std::chrono::high_resolution_clock::now();
		act_service_time = std::chrono::duration_cast<std::chrono::microseconds>(this->end_time - this->start_time).count();
		this->update_ts(act_service_time);
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

Context::Context(size_t context_id) : context_id(context_id){
	this->trace = new std::deque<Worker*>();
};

Context::~Context(){
	delete trace;
};

size_t Context::get_context_id(){
	return this->context_id;
}

size_t Context::get_n_threads(){
	return this->trace->size();
}

std::deque<Worker*>* Context::get_trace(){
	return this->trace;
}

void Context::move_in(Worker* w){
	this->trace->push_back(w);
	w->set_context(this->get_context_id());
	return;
}

Worker* Context::move_out(){
	Worker* w = this->trace->front(); 
	this->trace->pop_front();
	return w;
}


long Context::get_avg_ts(){
	long context_avg_ts = 0;
	for(auto worker : *(this->get_trace()))
		context_avg_ts+=worker->get_ts();
	return context_avg_ts/=this->get_n_threads();
}

/////////////////////////////////////////////////////////////////////////
//
//	Manager	
//
/////////////////////////////////////////////////////////////////////////


//il throughput per ocllector ed emitter, usiamo la varianza perchè Prendono il task e lo mettono da un'altra parte
Manager::Manager(long ts_goal,
		std::atomic<bool>* stop,
		Emitter* emitter,
		Collector* collector,
		std::vector<Worker*>* workers,
		size_t nw,
		size_t max_nw,
		size_t n_contexts,
		long sliding_size
		) : ts_goal(ts_goal), stop(stop), nw(nw), max_nw(max_nw), sliding_size(sliding_size), ProcessingElement(){
	std::ostringstream file_name_stream;
	this->nw_series =  new std::queue<size_t>();
	this->emitter = emitter;
	this->collector = collector;

	this->emitter->set_context(0);
	this->collector->set_context(0);
	this->set_context(0);
		
	for(auto id_context = 0; id_context < n_contexts; id_context++)
		this->idle_contexts.push_back(new Context(id_context));

	this->ts_lower_bound = this->ts_goal - this->ts_goal*2/10;
	this->ts_lower_bound = (this->ts_lower_bound < 0) ? 0 : this->ts_lower_bound;
	this->ts_upper_bound = this->ts_goal + this->ts_goal*2/10;

	Context* context;
	for(auto i = 0; i < nw; i++){
		context = this->idle_contexts.front();
		this->idle_contexts.pop_front();
		context->move_in((*workers)[i]);
		this->active_contexts.push_back(context);
	}
	for(auto i = nw; i < max_nw; i++) 
		this->active_contexts[i%nw]->move_in((*workers)[i]);

	file_name_stream << this->nw << "_" << this->max_nw << "_" << this->ts_goal << "_" << (*workers)[0]->get_in_buffer()->safe_get_size() << ".csv";
	this->data.open("./data/"+file_name_stream.str());
	if(this->data.is_open())
		this->data << this->ts_goal << "\n" << this->ts_upper_bound << "\n" << "Degree,Service_Time,Time\n";
	return;
}

Manager::~Manager(){
	delete this->stop;
	delete this->nw_series;
}

//questo deve semplicemente essere una piccola ccortezza. Se sono in una buona posizione ma
//mi rendo conto che la coda si sta sempre più ingrossando, allora evito di riempirla (c'ho anche il bound che è già buono. Quindi magari do un piccolo aumento di worker affinchè postponga il problema. Quindi serve un range erp il quale è stabile ma che non lo faccia diventare troppo veloce. In particolare devo definire dei largini per i quali quando è stabile, se detecta un bottleneck prova ad aumentare. Quindi anche se va più veloce del ts goal ma ritarda un bottleneck, aggiungi
bool Manager::detect_bottlenecks(){
	long acc = 0;
	double mean_ts_ws, ro;
	for(auto context : this->active_contexts)
		for(auto worker : *context->get_trace())
			acc += worker->get_ts();
	mean_ts_ws = static_cast<double>(acc)/this->max_nw;	
	ro = static_cast<double>(mean_ts_ws)/(this->emitter->get_ts()*this->max_nw);
	return (ro > 0.5) ? true : false;
}

void Manager::wake_worker(){
	if(this->idle_contexts.size() == 0) return;
	Worker* w = NULL; 
	Context *act_context, *to_wake;
	to_wake = this->idle_contexts.front();
	this->idle_contexts.pop_front();
	act_context = this->active_contexts.front();
	this->active_contexts.pop_front();
	w = act_context->move_out();
	to_wake->move_in(w);
	this->active_contexts.push_front(to_wake);	
	this->active_contexts.push_back(act_context);
	this->nw++;
	return;
}

void Manager::idle_worker(){ 
	if(this->active_contexts.size() == 1) return;
	Worker* w = NULL;
	Context *act_context, *to_idle_context;
	to_idle_context = this->active_contexts.back();
	this->active_contexts.pop_back();
	while(to_idle_context->get_n_threads() > 0){ 
		w = to_idle_context->move_out();
		act_context = this->active_contexts.front();
		act_context->move_in(w);
		this->active_contexts.pop_front();
		this->active_contexts.push_back(act_context); 
	}
	this->idle_contexts.push_back(to_idle_context);	
	this->nw--;
	return;
}




void Manager::control_nw_policy(long farm_ts){
	size_t nw = this->get_nw_moving_avg();
	if(farm_ts <= this->ts_upper_bound && farm_ts >= this->ts_lower_bound && this->detect_bottlenecks()){
		nw = ceil(static_cast<double>(farm_ts)/this->ts_lower_bound); 
		nw = (nw <= this->max_nw) ? nw : this->max_nw;
	}
	else if(farm_ts > this->ts_upper_bound){
		nw = ceil(static_cast<double>(farm_ts)/this->ts_lower_bound);
		nw = (nw <= this->max_nw) ? nw : this->max_nw;
	}
	else if(farm_ts < this->ts_lower_bound){
		nw = floor(static_cast<double>(this->ts_upper_bound)/farm_ts); 
		nw = (nw < this->active_contexts.size()) ? nw :this->active_contexts.size() - 1;
	}
	this->update_nw_moving_avg(nw);
	this->set_workers(this->get_nw_moving_avg());
	return;
}

void Manager::set_workers(size_t nw){
	if(nw > this->nw){
		size_t to_add = nw - this->nw;
		for(auto i = 0; i < to_add; i++)
			this->wake_worker();
	}
	else if(nw < this->nw){
		size_t to_remove = this->nw - nw;
		for(auto i = 0; i < to_remove; i++)
			this->idle_worker();
	}
	this->redistribute();
}

//i worker aumentato con la bottleneck mi vengono deschedulati perché act_farm_ts mi fa scattare l'idle 
//per risparmiare energie
//ho provato a controllare se se un dato core ci sono altre app ma sia attraverso il throughtput sia attraverso il service time, non riesco perchè se posiziono un'app sul medesimo core di dove sta già girando la farm, tutti i contesti decrementano in modo uguale le prestazioni. Inoltre controllando da htop se aggiungo un'app diminuisce il carico su un app e viene incrementato il clock ma nonostante questo l'applicazione dell'autonous sotto controllo rimane appesantita dalla seconda. Come se ...?
void Manager::body(){
	long farm_ts = 0, time = 0, rest = 200; //rand() 
	while(!(*this->stop)){
		time+=rest;
		std::this_thread::sleep_for(std::chrono::milliseconds(rest));
		farm_ts = this->get_service_time_farm();
		this->control_nw_policy(farm_ts);
		std::cout << "farm_ts: " << farm_ts << std::endl;
		std::cout << " >> " << nw << std::endl;

		if(this->data.is_open())
			this->data << this->nw << "," << farm_ts << "," << time << "\n";
	};
	data.close();
	return;
}


void Manager::redistribute(){
	size_t r = this->max_nw / this->nw;
	for(auto context : this->active_contexts)
		this->resize(context, r);
	Worker* w = NULL;
	for(auto context : this->active_contexts){
		while(context->get_n_threads() < r){
			w = this->ws_queue.front();
			this->ws_queue.pop_front();
			context->move_in(w);
		}
	}
	Context* context;
	while(!this->ws_queue.empty()){
		w = this->ws_queue.front();
		this->ws_queue.pop_front();
		this->active_contexts.front()->move_in(w);
		context = this->active_contexts.front();
		this->active_contexts.pop_front();
		this->active_contexts.push_back(context);
	}
	return;
}

void Manager::resize(Context* context, size_t size){
	while(context->get_n_threads() > size){
		Worker* w = context->move_out();
		this->ws_queue.push_front(w);
	}
	return;	
}



size_t Manager::get_nw_moving_avg(){
	return ceil(static_cast<double>(this->acc)/this->pos);
}

void Manager::update_nw_moving_avg(size_t new_value){
	this->acc += new_value;
	this->nw_series->push(new_value);
	if(this->pos <= this->sliding_size)
		this->pos++;	
	else{
		this->acc -= this->nw_series->front();
		this->nw_series->pop();
	}
	return;
}

long Manager::get_service_time_farm(){
	long contexts_ts_avg = 0;	
	for(auto context : this->active_contexts)
		contexts_ts_avg+=context->get_avg_ts();	
	contexts_ts_avg/=this->nw;
	return std::max({this->emitter->get_ts(),
			this->collector->get_ts(),
			contexts_ts_avg/static_cast<long>(this->nw)
			});
}

void Manager::run(){
	this->thread = new std::thread(&Manager::body, this);
	return;
}

void Manager::info(){
	std::cout << "\n***************" << std::endl;
	//for(auto context : this->active_contexts){
	//	std::cout << "ID: " << context->get_context_id() << " TS: " << context->get_mean_service_time() << " VAR: " << context->get_sd_service_time() << std::endl;
	//}
	std::cout << " ACTIVE " << std::endl;
	for(auto i = 0; i < this->active_contexts.size(); i++)
		std::cout << this->active_contexts[i]->get_context_id() << " - " << this->active_contexts[i]->get_n_threads() << std::endl;
	std::cout << " IDLE " << std::endl;
	for(auto i = 0; i < this->idle_contexts.size(); i++)
		std::cout << this->idle_contexts[i]->get_context_id() << " - " << this->idle_contexts[i]->get_n_threads() << std::endl;
	std::cout << " -------- " << std::endl;

	for(auto const& context : active_contexts){
		std::deque<Worker*>* trace = context->get_trace();
		for(auto const& w : *trace){
			std::cout << "Worker " << w->get_id() << ": " << w->get_ts()  << std::endl;
		}
	}

	return;
}


/*
//Assumo che i task siano distribuiti a caso perciò se la media di un core si distacca molto allora significa che c'è un'altra app sopra
void Manager::is_application_overlayed(){
	long diff = 0;
	for(auto context : this->active_contexts){
		diff = context->get_avg_ts() - this->get_contexts_avg_ts();
		long threshold = this->get_ts()*50/100;
		if( diff > 0 && diff > threshold)
			this->transfer_threads_to_idle_core(context);	
		this->update_contexts_stats();
	}
}

void Manager::transfer_threads_to_idle_core(Context*& from){
	if(this->idle_contexts.size() == 0)
		return;

	Context *swap, **to;
	std::deque<Worker*> *from_trace, *to_trace;

	from_trace = from->get_trace();
	to = &this->idle_contexts.front();
	to_trace = (*to)->get_trace();


	for(Worker* w : *from_trace)
		w->set_context((*to)->get_context_id());	
	from_trace->swap(*to_trace);
	
	swap = from;
	from = *to;
	*to = swap;

	return;
}


void Manager::update_contexts_stats(){
	long sum_avg_contexts = 0, context_avg_ts = 0;
	for(auto context : this->active_contexts)
		sum_avg_contexts+=context->get_avg_ts();	
	this->set_contexts_avg_ts(sum_avg_contexts/static_cast<long>(this->nw));
}

void Manager::set_contexts_avg_ts(long new_value){
	this->contexts_avg_ts = new_value;
	return;
}

long Manager::get_contexts_avg_ts(){
	return this->contexts_avg_ts;
}

*/
/////////////////////////////////////////////////////////////////////////
//
//	Autonomic Farm	
//
/////////////////////////////////////////////////////////////////////////


Worker* Autonomic_Farm::add_worker(std::vector<BUFFER*>* win_bfs, std::vector<BUFFER*>* wout_bfs, size_t buffer_len){ 
	Worker* worker = new Worker(this->fun_body, buffer_len);
	win_bfs->push_back(worker->get_in_buffer());
	wout_bfs->push_back(worker->get_out_buffer());
	(*this->workers).push_back(worker);
	return worker;
}

Autonomic_Farm::Autonomic_Farm(long ts_goal, size_t nw, size_t max_nw, std::function<ssize_t(ssize_t)> fun_body, size_t buffer_len, std::vector<ssize_t>* collection, long sliding_size = 100) : fun_body(fun_body){ 
	if(nw > max_nw){
		std::cout << "Error nw > max_nw" << std::endl;
		return;
	}
	if(ts_goal < 1) ts_goal = 1;

	size_t n_contexts = std::thread::hardware_concurrency();
	nw = (nw < n_contexts) ? nw : n_contexts; 
	this->max_nw = (max_nw < n_contexts) ? max_nw : n_contexts;
	std::cout << "Initial_degree: " << nw << std::endl;
	std::cout << "Max_degree " << this->max_nw << std::endl;
	this->stop = new std::atomic<bool>(false);
	std::vector<BUFFER*>* win_bfs = new std::vector<BUFFER*>();
	this->workers = new std::vector<Worker*>();
	std::vector<BUFFER*>* wout_bfs = new std::vector<BUFFER*>();	

	for(auto i = 0; i < this->max_nw; i++) 
		this->add_worker(win_bfs, wout_bfs, buffer_len);

	this->emitter = new Emitter(win_bfs, buffer_len, collection);
	this->collector = new Collector(wout_bfs, buffer_len);

	this->manager = new Manager(ts_goal,
			this->stop, this->emitter,
			this->collector, this->workers,
			nw, this->max_nw, n_contexts, sliding_size);
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

size_t Autonomic_Farm::pop_outputs(){
	void* task = 0;
	this->collector->get_out_buffer()->safe_pop(&task);
	size_t &t = (*((size_t*) task));
	return task == EOS ? -1 : t;
}
