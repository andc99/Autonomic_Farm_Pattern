USAGE = "----> Ex. ./main 10000 1 8 400 50 400 100 800 --- n_tasks start_degree max_degree ts_goal sma_window task_1 task_2 task_3" 

all:
	g++ -std=c++17 -pthread -O3 ./ff_main.cpp -o ff_main -I/home/fastflow
	@echo $(USAGE)

clean: 
	rm -f main
	@echo "Clean done"
