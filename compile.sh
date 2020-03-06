#!/bin/bash

# This script is used for TACC people to compile showq and showres on TACC systems
# Author: Si Liu
# Texas Advanced Computing Center

mymachine=Frontera

module load gcc/8.3.0
target=slurmshowq-3.5 #Frontera 07/07/2019

currenthost=`hostname -f`
if [[ $currenthost == *"ls5"* ]]
then
#Lonestar Only
	SLURM_LIB=/opt/slurm/default/lib64
	SLURM_INCLUDE=/opt/slurm/default/include
else
#Stampede, Maverick, or Wrangler
	SLURM_LIB=/usr/lib64
	SLURM_INCLUDE=/usr/include
fi


rm -rf slurm_showq.o showq
g++ -O3 -c slurm_showq.cpp -I$SLURM_INCLUDE -O3 -c slurm_showq.cpp -I$SLURM_INCLUDE    
g++ -I$SLURM_INCLUDE -L$SLURM_LIB -Wl,-rpath=$TACC_GCC_LIB64  -Wl,-rpath=$SLURM_LIB -lslurm main.cpp slurm_showq.o -o showq

/opt/apps/spp/gcc/9.1.0/bin/g++ -O3 -c slurm_showq.cpp -I$SLURM_INCLUDE -O3 -c slurm_showq.cpp -I$SLURM_INCLUDE    
/opt/apps/spp/gcc/9.1.0/bin/g++ -I$SLURM_INCLUDE -L$SLURM_LIB  -Wl,-rpath=$SLURM_LIB -lslurm main.cpp slurm_showq.o -o showq

#/usr/bin/g++ -O3 -c slurm_showq.cpp -I$SLURM_INCLUDE  
#/usr/bin/g++ -O3 -I$SLURM_INCLUDE -L$SLURM_LIB  -Wl,-rpath=$SLURM_LIB -lslurm main.cpp slurm_showq.o -o showq

chmod 755 showq
chmod 755 showres


date > timestamp
whoami >> timestamp
hostname -f  >> timestamp

#FRONTERA

mkdir -p ../slurm_showq_${mymachine}/${target}
cp showq ../slurm_showq_${mymachine}/${target}
cp showres ../slurm_showq_${mymachine}/${target}
cp timestamp ../slurm_showq_${mymachine}/${target}
cd ../slurm_showq_${mymachine}   
tar cvjSf ${target}.tar.gz ${target}
chmod 755 ${target}.tar.gz
