#include <iostream>
#include <vector>
#include <limits>
#include <queue>
#include <ff/ff.hpp>

using namespace ff;

const int MANAGER_ID = MAX_NUM_THREADS+100;

typedef enum {ADD, REMOVE} reconf_op_t;
struct Command_t{
	Command_t(int id, reconf_op_t op): id(id), op(op) {}
	int id;
	reconf_op_t op;
};

struct Seq: ff_node_t<size_t>{
	size_t n_tasks = 0;
	Seq(size_t n_tasks) : n_tasks(n_tasks){}

	size_t *svc(size_t *){
		for(size_t i = 0; i < n_tasks; i++){
			ff_send_out((size_t*) i);
		}
		return EOS;
	}

};

struct Worker: ff_monode_t<size_t>{
	std::function<size_t(size_t)> fun_body;

	Worker(std::function<size_t(size_t)> fun_body){
		this->fun_body = fun_body;
	}

	int svc_init(){
		printf("Worker id=%ld starting\n", get_my_id());
		return 0;
	}

	size_t* svc(size_t* task){
		size_t res = this->fun_body((size_t) task);
		//printf("Worker id=%ld got %ld\n", get_my_id(), (size_t)task);
		ff_send_out_to((size_t*) res, 1); //Task to Collector
		ff_send_out_to(task, 0); //Ready msg to Emitter
		return this->GO_ON;
	}

	void eosnotify(ssize_t){
		printf("Worker id=%ld received EOS\n", get_my_id());
	}

	void svc_end(){
		printf("Worker id=%ld going to sleep\n", get_my_id());
	}
};

struct Emitter: public ff_monode_t<size_t>{

	Emitter(int max_nw){
		this->max_nw = max_nw; //deve essere modulo hardware concurency
	}

	int svc_init(){
		for(int i = 0; i < get_num_outchannels(); i++)
			this->ready.push_back(i);
		for(int i = get_num_outchannels(); i < this->max_nw - get_num_outchannels(); i++) //worker sleeping
			this->sleeping.push_back(i);
		return 0;
	}

	size_t* svc(size_t* task){

		int wid = get_channel_id(); //sto mettendo size_t solo per coerenza con il mio. Il Torqua ha detto che int basta (20:00 circa)
		if((size_t) wid == MANAGER_ID){ 
			Command_t *cmd = reinterpret_cast<Command_t*>(task);
			printf("EMITTER2 SENDING %s to WORKER %d\n", cmd->op==ADD?"ADD":"REMOVE", cmd->id);
			switch(cmd->op) {
				case ADD:     {
						      ff_monode::getlb()->thaw(cmd->id, true);
						      assert(sleeping[cmd->id]);
						      sleeping[cmd->id] = false;
						      //frozen--;
					      } break;
				case REMOVE:  {
						      ff_send_out_to(GO_OUT, cmd->id);
						      assert(!sleeping[cmd->id]);
						      sleeping[cmd->id] = true;
						      //frozen++;
					      } break;
				default: abort();
			}
			delete cmd;            
			return GO_ON;
		}


		if(wid == -1){ //task ricevuto dal seq. Se nessun worker è pronto, prendo il dato e lo metto in un vettore e glielo passo poi appena uno mi dà un ack
			if(!this->ready.empty()){
				int chosen_w = this->ready.front(); 
				this->ready.pop_front();
				ff_send_out_to(task, chosen_w);
				this->on_the_fly++;    //mi serve?
			}else{
				this->data.push(task);
			}
			return GO_ON;

		}

		if((size_t) wid < get_num_outchannels()){ //feeback from worker
			//delete task; //cancello il task ricevuto indietro dal worker
			this->sleeping.push_front(wid);
			if(!data.empty()){
				size_t old_task = (size_t) data.front();
				ff_send_out_to((size_t*) old_task, wid);
				this->sleeping.pop_front();
				this->ready.push_front(wid);
			}
			return GO_ON;
		}


		if(eos_received && (on_the_fly<=0) && (this->sleeping.size()==max_nw)){ //manca roba
			printf("Emitter Exiting\n");
			return EOS;
		}
		return GO_ON;
	}

	void svc_end(){
		assert(data.size()==0);
	}

	void eosnotify(ssize_t id){

	}

	bool eos_received = 0;
	unsigned freezing = 0, on_the_fly = 0;
	int max_nw = 0;
	std::deque<int> ready;
	std::deque<int> sleeping;
	std::queue<size_t*> data;
};

struct Manager: ff_node_t<Command_t>{
	Manager(): channel(100, true, MANAGER_ID) {} //quel 100 e quel true che significano?

	Command_t* svc(Command_t *){ //deve poter accedere agli altri worker, (passare per parametro) gestire stability problem sma
		channel.ff_send_out(EOS);
		return this->GO_ON; //non è che termina alla prima iterazione?
	}

	void svc_end(){
		printf("Manager stopped\n");
	}

	int run(bool=false){
		return ff_node_t<Command_t>::run();
	}

	int wait(){
		return ff_node_t<Command_t>::wait();
	}

	ff_buffernode* const getChannel() {return &channel;}

	ff_buffernode channel;
};

struct Collector: ff_minode_t<size_t>{
	size_t* svc(size_t* task){
		printf("Collector received task = %ld, sending it batk to the Emittter\n", (size_t)(task));
		return task;
	}

	void eosnotify(ssize_t){
		printf("Collector received EOS\n");
	}
};




size_t prova(size_t t){
	return t++;
}

int main(int argc, char*[]){
	
	const int nw = 4;
	const int max_nw = 8;
	const size_t n_tasks = 1000;

	Seq seq(n_tasks);

	Manager manager;

	std::vector<ff_node*> W;
	for(size_t i = 0; i < nw; i++)
		W.push_back(new Worker(prova));
	ff_farm autonomic_farm(W);
	autonomic_farm.cleanup_workers(); //ca fa?

	//il Manager non viene aggiunto alla farm quindi ha un channel artificioso al quale andiamo ad assegnare un id che poi sarà utilizzato dall'Emitter per sapere da chi ha ricevuto 
	//gli id da nw sono positivi e sono i forward channel
	//Il backward channel dal Collector all'Emitter ha id = -2 (negativo)
	//-1 indica che viene dall'esterno della farm (24:00)
	//dopo dice che da 0 a nw-1 sono feedback channel dai worker

	autonomic_farm.getlb()->addManagerChannel(manager.getChannel());
	
	Emitter emitter(max_nw);

	autonomic_farm.remove_collector(); //può essere tolto
	autonomic_farm.add_emitter(&emitter);
	autonomic_farm.wrap_around();

	Collector collector;
	autonomic_farm.add_collector(&collector);
	autonomic_farm.wrap_around();

	ff_Pipe<> pipe(seq, autonomic_farm);

	if(pipe.run_then_freeze() < 0){
		error("running pipe\n");
		return -1;
	}

	manager.run();

	pipe.wait_freezing(); //wait the termination of the application
	
	pipe.wait();
	manager.wait();


	return 0;
}
