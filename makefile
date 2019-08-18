CFLAGS = -pthread -std=c++17 -O3 
CC = g++

all:
	$(CC) $(CFLAGS) ./buffers/circular_buffer.cpp ./buffers/buffer.cpp ./buffers/lock_buffer.cpp ./buffers/free_buffer.cpp ./buffers/safe_queue.cpp ./buffers/free_circular_buffer.cpp ./autonomic_farm.cpp ./main.cpp -o main	
	@echo "----> Ex. ./main 100 4 8 10 1" 

cb:
	$(CC) $(CFLAGS) ./buffers/circular_buffer.cpp ./buffers/buffer.cpp ./buffers/lock_buffer.cpp ./buffers/free_buffer.cpp ./buffers/safe_queue.cpp ./buffers/free_circular_buffer.cpp ./autonomic_farm.cpp ./main.cpp -o main -D CB	
	@echo "----> Ex. ./main 100 4 8 10 1"

sq:
	$(CC) $(CFLAGS) ./buffers/circular_buffer.cpp ./buffers/buffer.cpp ./buffers/lock_buffer.cpp ./buffers/free_buffer.cpp ./buffers/safe_queue.cpp ./buffers/free_circular_buffer.cpp ./autonomic_farm.cpp ./main.cpp -o main -D SQ	
	@echo "----> Ex. ./main 100 4 8 10 1" 

clean: 
	rm -f main
	@echo "Clean done"
