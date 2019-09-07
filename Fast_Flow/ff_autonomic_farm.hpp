#include <iostream>
#include <vector>
#include <ff/ff.hpp>
#include <ff/farm.hpp>
#include <ff/pipeline.hpp>

using namespace ff;

typedef enum {ADD, REMOVE} reconf_op_t;
struct Command_t{
	Command_t(size_t id, reconf_op_t op): id(id), op(op) {}
	size_t id;
	reconf_op_t op;
};

struct Seq: ff_node_t<size_t>{
	size_t n_tasks = 0;
	Seq(size_t n_tasks) : n_tasks(n_tasks){}

	size_t *svc(size_t *){
		for(size_t i = 0; i < n_tasks; i++)
			ff_send_out((size_t*) i);
		return EOS;
	}

};


struct Worker: ff_monode_t<size_t>{
	int svc_init(){
		printf("Worker id=%ld starting\n", get_my_id());
		return 0;
	}

	size_t* svc(size_t* task){
		printf("Worker id=%ld got %ld\n", get_my_id(), (size_t)task);
		ff_send_out_to(task, 1); //Task to Collector
		ff_send_out_to(task, 0); //Ready msg to Emitter
		return GO_ON;
	}

	void eosnotify(ssize_t){
		printf("Worker id=%ld received EOS\n", get_my_id());
	}

	void svc_end(){
		printf("Worker id=%ld going to sleep\n", get_my_id());
	}
};

struct Emitter: ff_monode_t<size_t>{

	int svc_init(){
				return 0;
	}

	size_t* svc(size_t* task){
		size_t wid = get_channel_id(); //sto mettendo size_t solo per coerenza con il mio. Il Torqua ha detto che int basta (20:00 circa)
		if((size_t) wid == 100){ //al posto di 100 ci va il manager ID
			return GO_ON;
		}


		if(wid == -1){ //task ricevuto dal seq. Se nessun worker è pronto, prendo il dato e lo metto in un vettore e glielo passo poi appena uno mi dà un ack
			return GO_ON;

		}

		if((size_t) wid < get_num_outchannels()){ //feeback from worker
			return GO_ON;
		}


		if(eos_received){ //manca roba
			return EOS;
		}
		return GO_ON;
	}

	void svc_end(){
	}

	void eosnotify(ssize_t id){

	}

	bool eos_received = 0;
};

struct Manager: ff_node_t<Command_t>{
	Manager(size_t nw): channel(100, true, nw+1) {} //quel 100 e quel true che significano?

	Command_t* svc(Command_t *){ //deve poter accedere agli altri worker, (passare per parametro) gestire stability problem sma
		struct timespec reg = {0, static_cast<long>(2*100L)};
		nanosleep(&reg, NULL);


		//boh---
		channel.ff_send_out(EOS);
		return GO_OUT; //non è che termina alla prima iterazione?
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


int main(int argc, char*[]){
	
	const size_t nw = 8;
	const size_t n_tasks = 1000;

	Seq seq(n_tasks);

	std::vector<ff_node*> W;
	for(size_t i = 0; i < nw; i++)
		W.push_back(new Worker);
	ff_farm autonomic_farm(W);
	autonomic_farm.cleanup_workers(); //ca fa?

	//il Manager non viene aggiunto alla farm quindi ha un channel artificioso al quale andiamo ad assegnare un id che poi sarà utilizzato dall'Emitter per sapere da chi ha ricevuto 
	//gli id da nw sono positivi e sono i forward channel
	//Il backward channel dal Collector all'Emitter ha id = -2 (negativo)
	//-1 indica che viene dall'esterno della farm (24:00)
	//dopo dice che da 0 a nw-1 sono feedback channel dai worker
	Manager manager(W.size());

	autonomic_farm.getlb()->addManagerChannel(manager.getChannel());
	
	Emitter emitter;

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
