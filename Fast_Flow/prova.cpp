#include <thread>
#include <map>
#include <chrono>
#include <atomic>
#include <iostream>
#include <ff/ff.hpp>
#include <functional>

using namespace ff;

// this is an id greater than all ids


long prova(long t){
	return ++t;
}

long busy_wait(long time){
	long act = 0;
	std::chrono::high_resolution_clock::time_point start_time = std::chrono::high_resolution_clock::now();
	while(act <= time){
		std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();
		act = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
		continue;
	}
	return 1;
}


// first stage
struct Seq: ff_node_t<long> {
	Seq(long n_tasks):n_tasks(n_tasks) {}

	int svc_init(){
		long t1 = 400;
		long t2 = 100;
		long t3 = 800;
		for(long i = 0; i < this->n_tasks/3; i++)
			collection.push_back(t1);
		
		for(long i = n_tasks/3; i < this->n_tasks*2/3; i++)
			collection.push_back(t2);
		
		for(long i = n_tasks*2/3; i < this->n_tasks; i++)
			collection.push_back(t3);
		
		return 0;
	}

	long *svc(long *) {
		for(long i=1;i<= this->n_tasks; ++i) {
			ff_send_out(new long(i));
		}
		return EOS;
	}
	
	void svc_end(){
		std::cout << " ME NE VO PURE IO " << std::endl;
	}
	long n_tasks=0;
	std::vector<long> collection;
};

// multi-input stage
struct Collector: ff_minode_t<long> {
	int svc_init(){
		printf("COLLECTOR %ld running on the core %ld\n", get_my_id(), ff_getMyCore());
	}
	long* svc(long* task) {

		this->start_time = std::chrono::high_resolution_clock::now();
		//printf("Collector received task = %ld, sending it back to the Emitter\n", (long)(task));
		this->end_time = std::chrono::high_resolution_clock::now();
		this->service_time = std::chrono::duration_cast<std::chrono::microseconds>(this->end_time - this->start_time).count();
		//delete task;
		return task;// (long*)service_time;

	}
	void eosnotify(ssize_t) {
		printf("Collector received EOS\n");
	}

	void svc_end() {
		printf("Collector SLEEEEEEEEEEEEEEEEEEP\n");
	}

	long get_ts(){
		return this->service_time;
	}

	std::chrono::high_resolution_clock::time_point start_time, end_time;	
	std::atomic<long> service_time = 0;

};

// scheduler 
class Emitter: public ff_monode_t<long> {
	public:

		long get_Tw(){
			long Tw = this->ts_active_nw.size();
			for(auto w : this->ts_active_nw){
				Tw += w.second;
				std::cout<< w.first << " : "  << w.second << std::endl;
			}
			std::cout << "TS FARM: " << Tw/this->ts_active_nw.size() << std::endl;
			return Tw/this->ts_active_nw.size(); 	
		}

		long get_service_time_farm(){
			return std::max({this->emitter_service_time,
					this->collector->get_ts(),
					this->get_Tw()
					});
		}


		Emitter(unsigned max_nw, unsigned nw, long ts_goal, long sliding_size, Collector* c){// ff_loadbalancer *const lb) : lb(lb){
			this->max_nw = max_nw;
			this->nw = nw;
			this->ts_goal = ts_goal;
			this->ts_lower_bound = this->ts_goal - this->ts_goal*2/10;
			this->ts_lower_bound = (this->ts_lower_bound < 0) ? 0 : this->ts_lower_bound;
			this->ts_upper_bound = this->ts_goal + this->ts_goal*2/10;
			this->collector = c;
			this->nw_series = new std::queue<long>();
			this->sliding_size = sliding_size;
			this->ready = new std::deque<int>();
			this->sleeping = new std::deque<int>();
		}

		void add_worker(){
			if(this->sleeping->empty()) return;
			int chosen_nw = this->sleeping->front();
			ff_monode::getlb()->thaw(chosen_nw, true);
			this->sleeping->pop_front();
			this->ready->push_front(chosen_nw);
			std::cout << "ADD wid " << chosen_nw << std::endl;
		}

		void remove_worker(){
			if(this->ts_active_nw.size() <= 1) return; //1 deve essere srempre attivo peròò
			int chosen_nw = this->ready->front();
			ff_send_out_to(GO_OUT, chosen_nw);
			ff_monode::getlb()->wait_freezing(chosen_nw);
			this->ready->pop_front();
			this->sleeping->push_front(chosen_nw);
			this->ts_active_nw.erase(chosen_nw);
			std::cout << "REMOVE wid " << chosen_nw << std::endl;
		}

		void update_nw_moving_avg(long new_value){
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

		long get_nw_moving_avg(){
			return this->acc/this->pos;
		}

		void threads_scheduling_policy(long new_nw){
			if(new_nw > this->ts_active_nw.size() &&  this->max_nw - new_nw <= this->sleeping->size()){
				long to_add = new_nw - this->ts_active_nw.size();
				for(auto i = 0; i < to_add; i++)
					this->add_worker();
			}
			else if(new_nw < this->ts_active_nw.size() && new_nw > 0){
				long to_remove = this->ts_active_nw.size() - new_nw;
				for(auto i = 0; i < to_remove; i++)
					this->remove_worker();
			}
		}

		void concurrency_throttling(){
			long Tw = this->get_Tw();
			long Te = this->emitter_service_time;
			long Tc = this->collector->get_ts();
			/*std::cout << " Tw " << Tw << std::endl;
			std::cout << " Te " << Te << std::endl;
			std::cout << " Tc " << Tc << std::endl;*/
			if(Tw < Te || Tw < Tc) return; //caso in cui il max non sia Tw non ci si può fare nulla
			long nw = this->ts_active_nw.size();
			if(Tw > this->ts_upper_bound){
				nw = Tw/this->ts_goal;
				nw = (nw <= this->max_nw) ? nw : this->max_nw;
			}
			else if(Tw < this->ts_lower_bound){
				nw = this->ts_goal/Tw; 
				nw = (nw < this->ts_active_nw.size()) ? nw : this->ts_active_nw.size()-1;
			}
			else if(Tw > this->ts_goal && Tw <= this->ts_upper_bound){
				nw = Tw/this->ts_goal;
				nw = (nw <= this->max_nw) ? nw : this->max_nw;
			}
			this->update_nw_moving_avg(nw);
			this->threads_scheduling_policy(this->get_nw_moving_avg());
			return;
		}

		bool check_concurrency(long time){
			this->end_interval = std::chrono::high_resolution_clock::now();
			long interval = std::chrono::duration_cast<std::chrono::milliseconds>(this->end_interval - this->start_interval).count();
			if(interval < time) return false;
			this->start_interval = std::chrono::high_resolution_clock::now();
			return true;
		}

		int svc_init() {
			for(int i = 0; i < this->max_nw; i++)
				ff_monode::getlb()->freeze(i);

			for(int i = 0; i < this->nw; i++){
				this->ready->push_back(i);
				this->ts_active_nw[i] = 1; //avoid floating point exception
				//ff_monode::getlb()->thaw(i,true);
			}
			int diff = this->max_nw - this->nw;
			for(int i = this->nw; i < this->max_nw; i++)//worker sleeping		
				ff_send_out_to(GO_OUT, i);

			for(int i = this->nw; i < this->max_nw; i++)//worker sleeping		
				 ff_monode::getlb()->wait_freezing(i);

			for(int i = this->nw; i < this->max_nw; i++){//worker sleeping		
				this->sleeping->push_back(i);	
			}
			printf("EMITTER %ld running on the core %ld\n", get_my_id(), ff_getMyCore());
			this->start_interval = std::chrono::high_resolution_clock::now();
			return 0;
		}



		long* svc(long* task) {       
			int wid = get_channel_id();
			this->start_time = std::chrono::high_resolution_clock::now();
			//if(check_concurrency(200)){
			//	this->get_service_time_farm();
			//	this->concurrency_throttling();
			//};
			if (wid == -1) { // task coming from seq
				//printf("Emitter: TASK FROM INPUT %ld \n", (long)t);
				if(!this->ready->empty()){
					int chosen_nw = this->ready->back();
		//			std::cout << chosen_nw << std::endl;
					ff_send_out_to(task, chosen_nw);
					std::cout << this->ready->size() << std::endl;
					onthefly++;
					this->ready->pop_front();
				}else{
		//			std::cout << "in coda" << std::endl;
					this->data.push(new long(*task)); //
				}
				this->end_time = std::chrono::high_resolution_clock::now();
				this->emitter_service_time = std::chrono::duration_cast<std::chrono::microseconds>(this->end_time - this->start_time).count();
				return GO_ON;
			}
			int p = wid;
			if ((size_t) wid < get_num_outchannels()) { // ack coming from the workers
			//	std::cout << "--------> " << *task << " : " << wid << std::endl;
				this->ts_active_nw[p] = std::move(*task); //rinominare in t
				this->ready->push_back(std::move(p));
				if (!this->data.empty()) {
					int next_w = this->ready->front();
					long invia = *this->data.front();
					ff_send_out_to(new long(invia), next_w);
					onthefly++;
					this->data.pop();
					this->ready->pop_front();
				} 
				delete task;

				this->end_time = std::chrono::high_resolution_clock::now();
				this->emitter_service_time = std::chrono::duration_cast<std::chrono::microseconds>(this->end_time - this->start_time).count();
				return GO_ON;
			}
			onthefly--;
			this->busy = this->max_nw - (this->ready->size() + this->sleeping->size());
			if(eos_received && this->busy == 0 && onthefly <= 0){ //se la somma è max size allora sono tutti fermi 
				printf("Emitter EXITING--------------------------------------------\n");
			/*	for(long i=0;i< this->ts_active_nw.size() ;++i)
            				ff_monode::getlb()->thaw(i,false);

				broadcast_task(EOS);
			*/
				this->end_time = std::chrono::high_resolution_clock::now();
				this->emitter_service_time = std::chrono::duration_cast<std::chrono::microseconds>(this->end_time - this->start_time).count();
				//std::cout << "nw " << this->ts_active_nw.size() << std::endl;
				return EOS;
			}

			this->end_time = std::chrono::high_resolution_clock::now();
			this->emitter_service_time = std::chrono::duration_cast<std::chrono::microseconds>(this->end_time - this->start_time).count();
			return GO_ON;
		}
		void svc_end() {
			std::cout << "BUONANOTTE " << std::endl;
			assert(data.size()==0);
		}

		void eosnotify(ssize_t id) {
			if (id == -1) { // we have to receive all EOS from the previous stage            
				eos_received = true;
				//printf("EOS received eos_received = %u nready = %u\n", eos_received, nready);
				//
				this->busy = this->max_nw - (this->ready->size() + this->sleeping->size());
				if (this->data.empty() && this->busy==0 && onthefly <= 0) {
					while(!this->sleeping->empty()){
						int wake_wid = this->sleeping->front();
						ff_monode::getlb()->thaw(wake_wid, false);
						this->sleeping->pop_front();
						std::cout << "ahahahahahahaha " << std::endl;

					}
					printf("EMITTER2 BROADCASTING EOS\n");
					broadcast_task(EOS);
				}
			} 

		}

		long get_ts(){
			return this->emitter_service_time;
		}

	private:
		bool eos_received = 0;
		unsigned nw, max_nw = 0, busy = 0, onthefly=0;
		std::deque<int>* ready;
		std::deque<int>* sleeping;
		std::queue<long*> data;
		std::unordered_map<int, long> ts_active_nw;
		long ts_goal = 1, ts_lower_bound = 1, ts_upper_bound = 1;
		long pos = 0, sliding_size = 1, acc = 1;
		std::queue<long>* nw_series;
		//ff_loadbalancer *const lb;

		Collector* collector;
		std::chrono::high_resolution_clock::time_point start_time, end_time;
		std::chrono::high_resolution_clock::time_point start_interval, end_interval;
		long emitter_service_time = 1;
};

struct Worker: ff_monode_t<long> {

	Worker(std::function<long(long)> fun_body){
		this->fun_body = fun_body;
	}

	int svc_init() {
		printf("I'm Worker %ld running on the core %ld\n", get_my_id(), ff_getMyCore());
		return 0;
	}

	long* svc(long* task) {
		//printf("I'm Worker%ld running on the core %ld\n", get_my_id(), ff_getMyCore());
		long &t = *task;
		this->start_time = std::chrono::high_resolution_clock::now();
		t =  this->fun_body(t);
		ff_send_out_to(task, 1); //Task to Collector
		this->end_time = std::chrono::high_resolution_clock::now();
		long service_time = std::chrono::duration_cast<std::chrono::microseconds>(this->end_time - this->start_time).count() + 1;
		ff_send_out_to(new long(t), 0); //(long*)service_time, 0); //Ready msg to Emitter
		return this->GO_ON;
	}

	void eosnotify(ssize_t) {
		printf("---Worker id=%ld received EOS\n", get_my_id());
	}

	void svc_end() {
		printf("Worker id=%ld going to sleep\n", get_my_id());
	}

	std::function<long(long)> fun_body;
	std::chrono::high_resolution_clock::time_point start_time, end_time;

};



int main(int argc, char* argv[]) {
	if(argc < 6){
		std::cout << "Missing Arguments.\nExample: " << argv[0] << " n_tasks nw max_nw ts_goal sliding_size" << std::endl;
		return 1;
	}
	long n_tasks = atoi(argv[1]); //string to unsigned int
	long nw = atoi(argv[2]);
	long max_nw = atoi(argv[3]);
	long ts_goal = atoi(argv[4]);
	long sliding_size = atoi(argv[5]);

	Seq seq(n_tasks);
	std::vector<ff_node*> W;

	ff_farm ff_autonomic_farm;
	for(long i=0;i<max_nw;++i)  
		W.push_back(new Worker(prova));
	ff_autonomic_farm.add_workers(W);
	ff_autonomic_farm.cleanup_workers();

	Collector C;
	Emitter E(max_nw, nw, ts_goal, sliding_size, &C); //, ff_autonomic_farm.getlb());



	// registering the manager channel as one extra input channel for the load balancer
	//farm.getlb()->addManagerChannel(manager.getChannel());


	ff_autonomic_farm.remove_collector();
	ff_autonomic_farm.add_emitter(&E); 
	ff_autonomic_farm.wrap_around();
	// here the order of instruction is important. The collector must be
	// added after the wrap_around otherwise the feedback channel will be
	// between the Collector and the Emitter
	ff_autonomic_farm.add_collector(&C);
	ff_autonomic_farm.wrap_around();

	ff_Pipe<> pipe(seq, ff_autonomic_farm);

	if (pipe.run_and_wait_end()<0) {
		error("running pipe\n");
		return -1;
	}            
	//pipe.wait_freezing();
	std::cout << "URCAAAAAAAAAAAAA" << std::endl;
	//ff_autonomic_farm.getlb()->waitlb();
	pipe.wait();

	return 0;
}

