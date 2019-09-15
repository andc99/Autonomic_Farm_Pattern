#!/bin/bash

echo -e "\nWARNING: Remember to set the FastFlow right include path\n" 

echo "-> Compiling Pthread..."
cd ./Pthread
make
echo -e "\n-> Pthread Test"
./main 1000000 1 256 4000 100 20 16000 4000 32000

echo -e "\n-> Compiling FastFlow..."
cd ./../Fast_Flow/
make
echo -e "\n-> FastFlow Test"
./ff_main 1000000 1 256 100 20 16000 4000 32000

echo -e "\n\n ----- End ----- "
