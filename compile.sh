#!/bin/bash

# This script is used for TACC people to compile showq and showres on TACC systems
# Author: Si Liu
# Texas Advanced Computing Center
# Last update: Apr 21, 2020


module load gcc
target=slurmshowq-3.6
machine=${TACC_SYSTEM}

# Version: 3.6, Si Liu, 04/21/2020 

currenthost=`hostname -f`

if [[ $currenthost == *"ls5"* ]]
then
#Lonestar Only
	SLURM_LIB=/opt/slurm/default/lib64
	SLURM_INCLUDE=/opt/slurm/default/include
else
#Stampede, Stampede2, Frontera, Maverick, or Wrangler
	SLURM_LIB=/usr/lib64
	SLURM_INCLUDE=/usr/include
fi

rm -rf slurm_showq.o showq
g++ -O3 -c slurm_showq.cpp -I$SLURM_INCLUDE -O3 -c slurm_showq.cpp -I$SLURM_INCLUDE    
g++ -I$SLURM_INCLUDE -L$SLURM_LIB -Wl,-rpath=$TACC_GCC_LIB64  -Wl,-rpath=$SLURM_LIB -lslurm main.cpp slurm_showq.o -o showq

chmod 755 showq
chmod 755 showres

date > timestamp
whoami >> timestamp
hostname -f  >> timestamp

mkdir -p ../slurm_showq_${machine}/${target}
cp showq ../slurm_showq_${machine}/${target}
cp showres ../slurm_showq_${machine}/${target}
cp timestamp ../slurm_showq_${machine}/${target}
cd ../slurm_showq_${machine}   
tar cvjSf ${target}.tar.gz ${target}
chmod 755 ${target}.tar.gz
