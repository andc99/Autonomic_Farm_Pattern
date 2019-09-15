#To test Pthread and FastFlow implementation
run the ./tests.sh


#To run Pthread implementation:
cd ./Pthread
make
./main n_tasks min_degree max_degree buffers_size ts_goal sma_window task_1 task_2 task_3

#---------------------------#

#To run FF implementation: 
#--> Makefile has to be change accordly the fastflow folder location
cd ./Fast_Flow
make
./ff_main n_tasks min_degree max_degree ts_goal sma_window task_1 task_2 task_3
