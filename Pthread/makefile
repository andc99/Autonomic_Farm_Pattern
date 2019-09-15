CFLAGS = -pthread -std=c++17 -O3
CC = g++
INCLUDE = ./buffers/buffer.cpp ./buffers/lock_buffer.cpp ./buffers/free_buffer.cpp ./processing_element.cpp ./emitter.hpp ./collector.hpp ./manager.hpp  ./worker.hpp ./data_structures/context.cpp ./autonomic_farm.hpp ./main.cpp 

USAGE = "----> Ex. ./main 10000 1 8 1000 400 50 400 100 800 / n_tasks start_degree max_degree buffers_size ts_goal sma_window task_1 task_2 task_3" 
 

sq:
	$(CC) $(CFLAGS) ./buffers/safe_queue.cpp $(INCLUDE) -o main -D SQ	
	@echo $(USAGE)

cb:
	$(CC) $(CFLAGS) ./buffers/circular_buffer.cpp $(INCLUDE) -o main -D CB	
	@echo $(USAGE)

free:
	$(CC) $(CFLAGS) ./buffers/free_circular_buffer.cpp $(INCLUDE)  -o main	
	@echo $(USAGE) "WARNING: Experimental Not Working"

clean: 
	rm -f main
	@echo "Clean done"
