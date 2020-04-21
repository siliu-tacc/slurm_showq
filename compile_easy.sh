#!/bin/bash

export SLURM_LIB=/lib64/slurm/
export SLURM_INCLUDE=/usr/include


rm -rf slurm_showq.o showq
/usr/bin/g++ -O3 -c slurm_showq.cpp -I$SLURM_INCLUDE  
/usr/bin/g++ -O3 -I$SLURM_INCLUDE -L$SLURM_LIB  -Wl,-rpath=$SLURM_LIB -lslurm main.cpp slurm_showq.o -o showq

