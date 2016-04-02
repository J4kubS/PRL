#!/usr/bin/env bash

if [ $# != 2 ]; then
	exit
fi

NUM_PROCESSORS=$2
NUM_NUMBERS=$1

PERFORMANCE_TEST=

PREFIX=/usr/local/share/OpenMPI
NUMBERS=numbers
SOURCE=mes.cpp
BINARY=mes

mpic++ -o $BINARY $SOURCE

if [ $PERFORMANCE_TEST ]; then
	> results.txt

	for ((n = 0; n < 100; n++)); do
		dd if=/dev/urandom of=$NUMBERS bs=$NUM_NUMBERS count=1 >& /dev/null
		mpirun -np $NUM_PROCESSORS $BINARY 2>> results.txt
	done
else
	dd if=/dev/urandom of=$NUMBERS bs=$NUM_NUMBERS count=1 >& /dev/null
	mpirun -np $NUM_PROCESSORS $BINARY
fi

rm -f $NUMBERS $BINARY
