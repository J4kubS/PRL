#!/usr/bin/env bash

if [ $# != 1 ]; then
	exit
fi

NUM_BITS=$1

PERFORMANCE_TEST=

# https://stackoverflow.com/questions/33659076/rounding-to-nearest-power-of-two-in-bash
NUM_PROCESSORS=`echo "x=$NUM_BITS; y=l(x)/l(2); scale=0; y/=1; z=2^y; if(x!=z) z=2^(y+1); 2*z-1;" | bc -l`

PREFIX=/usr/local/share/OpenMPI
NUMBERS=numbers
SOURCE=clapba.cpp
BINARY=clapba

if [ $PERFORMANCE_TEST ]; then
	> results.txt

	for ((n = 0; n < 10; n++)); do
		mpic++ -o $BINARY $SOURCE
		mpirun -np $NUM_PROCESSORS $BINARY 2>> results.txt
	done
else
	mpic++ -o $BINARY $SOURCE
	mpirun -np $NUM_PROCESSORS $BINARY | sort -n
fi
