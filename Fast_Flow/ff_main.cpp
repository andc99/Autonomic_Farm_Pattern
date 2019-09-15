#include <thread>
#include <set>
#include <math.h>
#include <map>
#include <chrono>
#include <iostream>
#include <functional>
#include <fstream>
#include <sstream>
#include <ff/ff.hpp>

using namespace ff;


//Some Tests functions
//
//////////////////////////////////
size_t isPrime(size_t x){
	if(x==2)
		return 1;
	if(x%2==0)
		return 0;
	size_t i = 2, sq = sqrt(x);
	while(i <= sq){
		if(x % i == 0)
			return 0;
		i++;
	}	
	return 1;
}


//////////////////////////////////
size_t busy_wait(size_t time){
	long act = 0;
	std::chrono::high_resolution_clock::time_point start_time = std::chrono::high_resolution_clock::now();
	while(act <= time){
		std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();
		act = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
		continue;
	}
	return 1;
}

//////////////////////////////////

// first stage
struct Seq: ff_node_t<size_t> {
	Seq(std::vector<size_t>* collection): collection(collection) {}

	int svc_init(){
		printf("Seq Started on core %ld\n", ff_getMyCore());
	}

	size_t *svc(size_t *) {
		for(auto i = 0; i < this->collection->size(); i++){
			ff_send_out(new size_t((*collection)[i]));
		}
		return EOS;
	}
	
	void svc_end(){
		printf("Seq Ends\n");
	}

	
	std::vector<size_t>* collection;
};

//////////////////////////////////

struct Collector: ff_minode_t<size_t> {
	int svc_init(){
		printf("Collector %ld running on the core %ld\n", get_my_id(), ff_getMyCore());
	}

	size_t* svc(size_t* task) {
		this->start_time = std::chrono::high_resolution_clock::now();
		this->end_time = std::chrono::high_resolution_clock::now();
		size_t service_time = std::chrono::duration_cast<std::chrono::microseconds>(this->end_time - this->start_time).count() + 1;
		return new size_t(service_time);

	}

	void eosnotify(ssize_t) {
		printf("Collector received EOS\n");
	}

	void svc_end() {
		printf("Collector Ends\n");
	}

	std::chrono::high_resolution_clock::time_point start_time, end_time;	

};

//////////////////////////////////

class Emitter: public ff_monode_t<size_t> {
	public:
		size_t get_Tw(){
			size_t Tw = this->ts_workers.size();
			for(auto w : this->ts_workers)
				Tw += w.second;	
			Tw/=this->ts_workers.size();
			std::cout << "FF_Farm Ts: " << Tw/this->ts_workers.size() << std::endl;
			std::cout << " Degree >> " << this->ts_workers.size() << std::endl;
			return Tw;
		}

		size_t get_service_time_farm(){
			return std::max({this->emitter_service_time,
					this->collector_service_time,
					this->get_Tw()/this->ts_workers.size()
					});
		}


		Emitter(size_t max_nw, size_t nw, long ts_goal, long sliding_size){
			this->max_nw = max_nw;
			this->nw = nw;
			this->ts_goal = ts_goal;

			//Define an interval of the ts_goal in which is not needed to change the parallelism degree to limit the stability problem
			this->ts_lower_bound = this->ts_goal - this->ts_goal*2/10;
			this->ts_lower_bound = (this->ts_lower_bound < 0) ? 0 : this->ts_lower_bound;
			this->ts_upper_bound = this->ts_goal + this->ts_goal*2/10;

			this->sliding_size = sliding_size;
		}

		void add_worker(){
			if(this->idle_workers.empty()) return; //If any sleeping contexts are available, return
			int wid = this->idle_workers.front();
			this->idle_workers.pop_front();
			ff_monode::getlb()->thaw(wid, true);
			this->ready.push_back(wid);
			this->active_workers.insert(wid);
			this->ts_workers[wid] = 1;
			printf("ADD wid %d\n", wid);
		}

		void remove_worker(){
			if(this->active_workers.size() <= 1) return; //at least one worker must be present
			int wid = -1;
			if(!this->ready.empty()){
				wid = this->ready.back(); 
				this->ready.pop_back();
			}else
				wid = *(this->active_workers.begin());
			ff_send_out_to(GO_OUT, wid);
			this->idle_workers.push_front(wid);
			this->active_workers.erase(wid);
			this->ts_workers.erase(wid);
			printf("REMOVE wid %d\n", wid);
		}

		//Update the SMA
		void update_nw_moving_avg(size_t new_value){
			this->acc += new_value;
			this->nw_series.push(new_value);
			if(this->pos <= this->sliding_size)
				this->pos++;	
			else{
				this->acc -= this->nw_series.front();
				this->nw_series.pop();
			}
			return;
		}

		size_t get_nw_moving_avg(){
			return this->acc/this->pos;
		}

		void threads_scheduling_policy(size_t new_nw){
			if(new_nw > this->active_workers.size() &&  this->max_nw - new_nw <= this->idle_workers.size()){
				int to_add = new_nw - this->ts_workers.size();
				for(auto i = 0; i < to_add; i++)
					this->add_worker();
			}
			else if(new_nw < this->active_workers.size() && new_nw > 0){
				int to_remove = this->ts_workers.size() - new_nw;
				for(auto i = 0; i < to_remove; i++)
					this->remove_worker();
			}
		}

		void concurrency_throttling(){
			long Tw = this->get_Tw();
			long Te = this->emitter_service_time;
			long Tc = this->collector_service_time;
			/*std::cout << " Tw " << Tw << std::endl;
			std::cout << " Te " << Te << std::endl;
			std::cout << " Tc " << Tc << std::endl;*/
			if(Tw < Te || Tw < Tc) return; //In the case that Tw is less than Collector Service_Time and Emitter Service_Time, the concurrency_throttling can't do nothing
			int nw = this->ts_workers.size();
			if(Tw > this->ts_upper_bound){ //Increase the degree -> Service_Time is getting higher than Ts_goal
				nw = Tw/this->ts_goal;
				nw = (nw <= this->max_nw) ? nw : this->max_nw; 
			}
			else if(Tw < this->ts_lower_bound){ //Decrese the degree -> Service_Time is getting lower than Ts_goal
				nw = this->ts_goal/Tw; 
				nw = (nw < this->ts_workers.size()) ? nw : this->ts_workers.size()-1;
			}
			this->update_nw_moving_avg(nw);
			this->threads_scheduling_policy(this->get_nw_moving_avg());
			return;
		}

		//Wait time to perform next degree adjustment
		bool check_concurrency(long time){
			this->end_interval = std::chrono::high_resolution_clock::now();
			long interval = std::chrono::duration_cast<std::chrono::milliseconds>(this->end_interval - this->start_interval).count();
			if(interval < time) return false;
			this->start_interval = std::chrono::high_resolution_clock::now();
			return true;
		}

		int svc_init() {
			std::ostringstream file_name_stream;
			file_name_stream << this->nw << "_" << this->max_nw << "_" << this->ts_goal <<".csv";
			this->to_save.open("./dataFF/"+file_name_stream.str());
			if(this->to_save.is_open())
					this->to_save << this->ts_goal << "\n" << this->ts_upper_bound << "\n"<< this->ts_lower_bound << "\n" << "Degree,Service_Time,Time" << "\n";

			for(int i = 0; i < this->nw; i++){
				this->ready.push_back(i);
				this->active_workers.insert(i);
				this->ts_workers[i] = 1; //avoid floating point exception
			}
			for(int i = this->nw; i < this->max_nw; i++)
				ff_send_out_to(GO_OUT, i);

			for(int i = this->nw; i < this->max_nw; i++){
				 ff_monode::getlb()->wait_freezing(i);
				 this->idle_workers.push_back(i);	
			}
			
			printf("Collector %ld running on the core %ld\n", get_my_id(), ff_getMyCore());
			this->start_interval = std::chrono::high_resolution_clock::now();
			return 0;
		}


		size_t* svc(size_t* task) {     
			int wid = get_channel_id();
			this->start_time = std::chrono::high_resolution_clock::now();
			if(check_concurrency(rest)){ //Fixed to 200milliseconds to compare with the pthread implementation
				long TsFarm = this->get_service_time_farm(); 
				//std::cout << "sleep " << this->idle_workers.size() << std::endl;
				//std::cout << "active_workers " << this->active_workers.size() << std::endl;
				//std::cout << "ts_workers " << this->ts_workers.size() << std::endl;
				if(this->to_save.is_open()) //Save data in csv file
					this->to_save << this->ts_workers.size() << "," << TsFarm << "," << time << "\n";
				time+=rest;
				this->concurrency_throttling();
			};
			if (wid == -1) { // task coming from seq
				if(this->ready.empty())
					this->data.push(task);
				else{
					int wid = this->ready.front();
					ff_send_out_to(task, wid); 
					this->onthefly++;
					this->ready.pop_front();
				}
				this->end_time = std::chrono::high_resolution_clock::now();
				this->emitter_service_time = std::chrono::duration_cast<std::chrono::microseconds>(this->end_time - this->start_time).count();
				return GO_ON;
			}
			if ((size_t) wid < get_num_outchannels()) { // Service_Time from Workers
				if(std::find(this->active_workers.begin(), this->active_workers.end(), wid) != this->active_workers.end()){
					this->ts_workers[wid] = (*task); 	
					this->ready.push_back(wid);
				}
				if(!this->data.empty() && !this->ready.empty()){
					int wid = this->ready.front();
					this->ready.pop_front();
					size_t* to_send = this->data.front(); 
					this->data.pop();
					ff_send_out_to(to_send, wid);
					this->onthefly++;
				}
				this->end_time = std::chrono::high_resolution_clock::now();
				this->emitter_service_time = std::chrono::duration_cast<std::chrono::microseconds>(this->end_time - this->start_time).count();
				return GO_ON;
			}
			this->collector_service_time = (*task); //In this case has been arrived the Collector's Service_Time
			this->onthefly--;
			delete task;
			if(eos_received && this->ready.size() == this->active_workers.size() && this->onthefly <= 0){ //Whether has been received the eos from seq and all workers are ready and there aren't pending task, then stop
				while(!this->idle_workers.empty()){
						int wid = this->idle_workers.front();
						ff_monode::getlb()->thaw(wid, true);
						this->idle_workers.pop_front();
				}
				broadcast_task(EOS);	//not needed
				return EOS;
			}

			this->end_time = std::chrono::high_resolution_clock::now();
			this->emitter_service_time = std::chrono::duration_cast<std::chrono::microseconds>(this->end_time - this->start_time).count();
			return GO_ON;
		}

		void svc_end() {
			to_save.close();
			printf("Emitter Ends\n");
		}

		void eosnotify(ssize_t id) {
			if (id == -1)
				eos_received = true;
		}
			


	private:
		bool eos_received = 0;
		size_t nw = 0, max_nw = 0, busy = 0;
		std::deque<int> idle_workers; //Sleeping Worker
		std::set<int> active_workers; //Active Workers
		std::deque<int> ready; //Worker waiting for a task
		std::queue<size_t*> data; //data to be send
		std::unordered_map<int, size_t> ts_workers; //workers service_time
		long ts_goal = 1, ts_lower_bound = 1, ts_upper_bound = 1, onthefly = 0;
		size_t pos = 0, sliding_size = 1, acc = 1;
		size_t emitter_service_time = 1, collector_service_time = 1;
		std::queue<int> nw_series;

		std::chrono::high_resolution_clock::time_point start_time, end_time;
		std::chrono::high_resolution_clock::time_point start_interval, end_interval;
		std::ofstream to_save;
		long rest = 200, time = 0;
};

//////////////////////////////////

struct Worker: ff_monode_t<size_t> {
	Worker(std::function<size_t(size_t)> fun_body){
		this->fun_body = fun_body;
	}

	int svc_init() {
		printf("Worker %ld running on the core %ld\n", get_my_id(), ff_getMyCore());
		return 0;
	}

	size_t* svc(size_t* task) {
		this->start_time = std::chrono::high_resolution_clock::now();
		size_t &t = *task;
		t =  this->fun_body(t);
		ff_send_out_to(task, 1); //Task to Collector
		this->end_time = std::chrono::high_resolution_clock::now();
		size_t service_time = std::chrono::duration_cast<std::chrono::microseconds>(this->end_time - this->start_time).count() + 1;
		ff_send_out_to(new size_t(service_time), 0); //Send Back to Emitter the Worker's Service_Time 
		return this->GO_ON;
	}

	void eosnotify(ssize_t) {
		printf("Worker id=%ld received EOS\n", get_my_id());
	}

	void svc_end() {
		printf("Worker id=%ld going to sleep\n", get_my_id());
	}

	std::function<size_t(size_t)> fun_body;
	std::chrono::high_resolution_clock::time_point start_time, end_time;

};

//////////////////////////////////

int main(int argc, char* argv[]) {
	if(argc < 8){
		std::cout << "Missing Arguments.\nExample: " << argv[0] << " n_tasks start_degree max_degree ts_goal sliding_size task_1 task_2 task_3" << std::endl;
		return 1;
	}
	size_t n_tasks = atoi(argv[1]);
	size_t nw = atoi(argv[2]);
	size_t max_nw = atoi(argv[3]);
	size_t ts_goal = atoi(argv[4]);
	size_t sliding_size = atoi(argv[5]);
	size_t task_1 = atoi(argv[6]);
	size_t task_2 = atoi(argv[7]);
	size_t task_3 = atoi(argv[8]);
	
	//Some checks to avoid malformed parameters
	if(nw > max_nw) nw = max_nw;
	if(ts_goal < 1) ts_goal = 1;
	size_t n_contexts = std::thread::hardware_concurrency();
	nw = (nw <= n_contexts) ? nw : n_contexts;
	max_nw = (max_nw <= n_contexts) ? max_nw : n_contexts;

	printf("Initial Degree: %ld\n", nw);
	printf("Max Degree: %ld\n", max_nw);

	std::vector<size_t> collection;
	for(size_t i = 0; i < n_tasks/3; i++)
		collection.push_back(task_1);

	for(size_t i = n_tasks/3; i < n_tasks*2/3; i++)
		collection.push_back(task_2);

	for(size_t i = n_tasks*2/3; i < n_tasks; i++)
		collection.push_back(task_3);

	Seq seq(&collection);
	std::vector<ff_node*> W;

	ff_farm ff_autonomic_farm;

	for(size_t i=0;i<max_nw;++i)  
		W.push_back(new Worker(busy_wait));

	ff_autonomic_farm.add_workers(W);
	ff_autonomic_farm.cleanup_workers();

	Emitter E(max_nw, nw, ts_goal, sliding_size); 
	ff_autonomic_farm.remove_collector();
	ff_autonomic_farm.add_emitter(&E); 
	ff_autonomic_farm.wrap_around(); //connects workers to emitter

	Collector C;
	ff_autonomic_farm.add_collector(&C);
	ff_autonomic_farm.wrap_around(); //connects collector to emitter

	std::chrono::high_resolution_clock::time_point start_time = std::chrono::high_resolution_clock::now();
	ff_Pipe<> pipe(seq, ff_autonomic_farm);

	if (pipe.run_then_freeze()<0) {
		error("running pipe\n");
		return -1;
	}            
	pipe.wait_freezing();
	pipe.wait();
	std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();
	long act = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
	std::cout << "FF TIME " << act << std::endl;

	return 0;
}

