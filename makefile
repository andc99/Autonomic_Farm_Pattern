CFLAGS = -pthread -std=c++17 -O3
CC = g++
INCLUDE = ./buffers/buffer.cpp ./buffers/lock_buffer.cpp ./buffers/free_buffer.cpp ./processing_element.cpp ./emitter.hpp ./collector.hpp ./manager.hpp  ./worker.hpp ./data_structures/context.cpp ./autonomic_farm.hpp ./main.cpp 

USAGE = "----> Ex. ./main 100 4 8 10 1" 
 
all:
	$(CC) $(CFLAGS) ./buffers/free_circular_buffer.cpp $(INCLUDE)  -o main	
	@echo $(USAGE)
cb:
	$(CC) $(CFLAGS) ./buffers/circular_buffer.cpp $(INCLUDE) -o main -D CB	
	@echo $(USAGE)

sq:
	$(CC) $(CFLAGS) ./buffers/safe_queue.cpp $(INCLUDE) -o main -D SQ	
	@echo $(USAGE)

clean: 
	rm -f main
	@echo "Clean done"
