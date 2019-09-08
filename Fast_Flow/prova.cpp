#include <iostream>
#include <ff/ff.hpp>
#include <functional>

using namespace ff;

// this is an id greater than all ids
const int MANAGERID = MAX_NUM_THREADS+100;

size_t prova(size_t t){
	return ++t;
}

typedef enum { ADD, REMOVE } reconf_op_t;
struct Command_t {
	Command_t(reconf_op_t op): op(op) {}
	reconf_op_t op;
};

// first stage
struct Seq: ff_node_t<long> {
	long ntasks=0;
	Seq(long ntasks):ntasks(ntasks) {}

	long *svc(long *) {
		for(long i=1;i<=ntasks; ++i) {
			ff_send_out((long*)i);

			struct timespec req = {0, static_cast<long>(5*1000L)};
			nanosleep(&req, NULL);
		}
		return EOS;
	}
};

// scheduler 
class Emitter: public ff_monode_t<long> {
	public:

		Emitter(unsigned max_nw){
			this->max_nw = max_nw;
		}

		int svc_init() {
			for(int i = 0; i < get_num_outchannels(); i++)
				this->ready.push_back(i);
			int diff = this->max_nw - get_num_outchannels();
			for(int i = get_num_outchannels(); i < this->max_nw; i++)//worker sleeping
				this->sleeping.push_back(i);	
			return 0;
		}
		long* svc(long* task) {       
			int wid = get_channel_id();
			//std::cout << this->sleeping.size() << " + " << this->ready.size() << " = " << this->ready.size()+this->sleeping.size() << " / " << this->max_nw << std::endl;
			// the id of the manager channel is greater than the maximum id of the workers
			//std::cout << "S:" << this->sleeping.size() << " + R:" << this->ready.size() << " = " << this->ready.size()+this->sleeping.size() << " / " << this->max_nw << std::endl;
			if ((size_t)wid == MANAGERID) {  
				Command_t *cmd = reinterpret_cast<Command_t*>(task);
				int chosen_nw = -1;
				switch(cmd->op) {
					case ADD:     {
							      if(this->sleeping.empty()) abort();
							      chosen_nw = this->sleeping.front();
							      ff_monode::getlb()->thaw(chosen_nw, true);
							      this->sleeping.pop_front();
							      this->ready.push_front(chosen_nw);
						      } break;
					case REMOVE:  {
							      if(this->ready.empty()) abort(); //1 deve essere srempre attivo peròò
							      chosen_nw = this->ready.front();
							      ff_send_out_to(GO_OUT, chosen_nw);
							      this->ready.pop_front();
							      this->sleeping.push_front(chosen_nw);
						      } break;
					default: abort();
				}
				printf("EMITTER2 SENDING %s to WORKER %d\n", cmd->op==ADD?"ADD":"REMOVE", chosen_nw);
				delete cmd;            
				return GO_ON;
			}

			if (wid == -1) { // task coming from seq
				//printf("Emitter: TASK FROM INPUT %ld \n", (long)t);
				if(!this->ready.empty()){
					int chosen_nw = this->ready.front();
					this->ready.pop_front();
					ff_send_out_to(task, chosen_nw);
				}else{
					this->data.push_front(task); //
				}
				return GO_ON;
			}

			if ((size_t)wid < get_num_outchannels()) { // ack coming from the workers
				//delete task;
				this->ready.push_front(wid);
				if (!this->data.empty()) {
					ff_send_out_to(this->data.front(), wid);
					this->data.pop_front();
					this->ready.pop_front();
				} 
				return GO_ON;
			}
			
			// task coming from the Collector
			//printf("Emitter got %ld back from COLLECTOR data.size=%ld, onthefly=%d\n", (long)t, data.size(), onthefly);
			this->busy = this->max_nw - (this->ready.size() + this->sleeping.size());
			if(eos_received && !this->busy){ //se la somma è max size allora sono tutti fermi 
				printf("Emitter EXITING--------------------------------------------\n");
				return EOS;
			}
			return GO_ON;
		}
		void svc_end() {
			// just for debugging
			assert(data.size()==0);
		}

		void eosnotify(ssize_t id) {
			if (id == -1) { // we have to receive all EOS from the previous stage            
				eos_received = true;
				//printf("EOS received eos_received = %u nready = %u\n", eos_received, nready);
				//
				this->busy = this->max_nw - (this->ready.size() + this->sleeping.size());
				if (this->data.empty() && !this->busy) {
					while(!this->sleeping.empty()){
						int wake_wid = this->sleeping.front();
						this->sleeping.pop_front();
					}
					printf("EMITTER2 BROADCASTING EOS\n");
					broadcast_task(EOS);
				}
			} 

		}

	private:
		bool eos_received = 0;
		unsigned max_nw = 0, busy = 0;
		std::deque<int> ready;
		std::deque<int> sleeping;
		std::deque<long*> data;
};

struct Worker: ff_monode_t<long> {

	Worker(std::function<size_t(size_t)> fun_body){
		this->fun_body = fun_body;
	}

	int svc_init() {
		printf("Worker id=%ld starting\n", get_my_id());
		return 0;
	}

	long* svc(long* task) {
		long res =  this->fun_body((long)task);
		ff_send_out_to((long*)res, 1); //Task to Collector
		ff_send_out_to(task, 0); //Ready msg to Emitter
		return this->GO_ON;
	}

	void eosnotify(ssize_t) {
		printf("Worker2 id=%ld received EOS\n", get_my_id());
	}

	void svc_end() {
		//printf("Worker2 id=%ld going to sleep\n", get_my_id());
	}

	std::function<size_t(size_t)> fun_body;

};

// multi-input stage
struct Collector: ff_minode_t<long> {
	long* svc(long* task) {
		//printf("Collector received task = %ld, sending it back to the Emitter\n", (long)(task));
		return task;
	}
	void eosnotify(ssize_t) {
		printf("Collector received EOS\n");
	}

};


struct Manager: ff_node_t<Command_t> {
	Manager(): 
		channel(100, true, MANAGERID) {}

	Command_t* svc(Command_t *) {

		struct timespec req = {0, static_cast<long>(5*1000L)};
		nanosleep(&req, NULL);

		Command_t *cmd1 = new Command_t(REMOVE);
		channel.ff_send_out(cmd1);

		Command_t *cmd2 = new Command_t(REMOVE);
		channel.ff_send_out(cmd2);

		{
			struct timespec req = {0, static_cast<long>(5*1000L)};
			nanosleep(&req, NULL);
		}

		Command_t *cmd3 = new Command_t(ADD);
		channel.ff_send_out(cmd3);

		Command_t *cmd4 = new Command_t(ADD);
		channel.ff_send_out(cmd4);

		{
			struct timespec req = {0, static_cast<long>(5*1000L)};
			nanosleep(&req, NULL);
		}

		channel.ff_send_out(EOS);

		return GO_OUT;
	}

	void svc_end() {
		printf("Manager ending\n");
	}


	int run(bool=false) { return ff_node_t<Command_t>::run(); }
	int wait()          { return ff_node_t<Command_t>::wait(); }


	ff_buffernode * const getChannel() { return &channel;}

	ff_buffernode  channel;
};



int main(int argc, char* argv[]) {

	unsigned nworkers = 3;
	int ntasks = 1000;
	if (argc>1) {
		if (argc < 3) {
			std::cerr << "use:\n" << " " << argv[0] << " numworkers ntasks\n";
			return -1;
		}
		nworkers  =atoi(argv[1]);
		ntasks    =atoi(argv[2]);
		if (ntasks<500) ntasks = 500;
		if (nworkers <3) nworkers = 3;
	}

	unsigned max_nw = 8; //check hardconc
	Seq seq(ntasks);
	Manager manager;

	std::vector<ff_node* > W;
	for(size_t i=0;i<nworkers;++i)  
		W.push_back(new Worker(prova));
	ff_farm farm(W);
	farm.cleanup_workers();

	// registering the manager channel as one extra input channel for the load balancer
	farm.getlb()->addManagerChannel(manager.getChannel());

	Emitter E(max_nw);

	farm.remove_collector();
	farm.add_emitter(&E); 
	farm.wrap_around();
	// here the order of instruction is important. The collector must be
	// added after the wrap_around otherwise the feedback channel will be
	// between the Collector and the Emitter
	Collector C;
	farm.add_collector(&C);
	farm.wrap_around();

	ff_Pipe<> pipe(seq, farm);

	if (pipe.run_then_freeze()<0) {
		error("running pipe\n");
		return -1;
	}            
	manager.run();
	pipe.wait_freezing();
	pipe.wait();
	manager.wait();

	return 0;
}

