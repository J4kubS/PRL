#include <iostream>
#include <fstream>
#include <vector>
#include <math.h>
#include <mpi.h>

// Uncomment to disable all stdout output and enable outputting
// run time of root (in seconds) to stderr.
//#define PERFORMANCE_TEST

using namespace std;

const string NUMBERS_FILE_NAME = "numbers";

// Assuming that the supplied numbers will be all >= 0
const int PADDING_VALUE = -1;
const int EMPTY_VALUE = -2;
const int QUIT_VALUE = -3;

const int VALUE_TAG = 1;

inline void receiveValue(int rank, int *value) {
	MPI_Recv(value, 1, MPI_INT, rank, VALUE_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

inline void sendValue(int rank, int *value) {
	MPI_Send(value, 1, MPI_INT, rank, VALUE_TAG, MPI_COMM_WORLD);
}

vector<int> loadValues() {
	vector<int> values;
	fstream input;
	int value;

	input.open(NUMBERS_FILE_NAME.c_str(), ios::in);

	while (input.good()) {
		value = input.get();

		// Skip EOF
		if (input.good()) {
			values.push_back(value);
		}
	}

	input.close();

	return values;
}

void padValues(vector<int> *values, int target) {
	for (int i = values->size(); i < target; i++) {
		values->push_back(PADDING_VALUE);
	}
}

void printValues(vector<int> *values) {
	for (int i = 0; i < values->size(); i++) {
		cout << (i > 0 ? " " : "") << (int) values->at(i);
	}

	cout << endl;
}

void sendValues(vector<int> *values) {
	int size = values->size();

	for (int i = 0; i < size; i++) {
		int value = values->at(i);
		int rank = size + i - 1;

		sendValue(rank, &value);
	}
}

int main(int argc, char *argv[]) {
	int size;
	int rank;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	// https://en.wikipedia.org/wiki/Binary_heap
	// We'll use ranks as array indexes
	int root = 0;
	int firstLeaf = floor(size / 2);
	int parent = floor((rank - 1) / 2);
	int rightChild = 2 * rank + 2;
	int leftChild = 2 * rank + 1;

	int rightValue = EMPTY_VALUE;
	int leftValue = EMPTY_VALUE;
	int value = EMPTY_VALUE;

	bool isRoot = rank == root;
	bool isNode = ! isRoot && rank < firstLeaf;
	bool isLeaf = ! isNode && rank >= firstLeaf;

	bool isRightEmpty;
	bool isLeftEmpty;
	bool extract = true;
	bool quit = false;

	// Print, pad, and distribute values
	if (isRoot) {
		vector<int> values = loadValues();
		// Assuming that the size is provided correctly
		int target = (size + 1) / 2;

		#ifndef PERFORMANCE_TEST
		printValues(&values);
		#endif

		padValues(&values, target);

		// Edge case
		if (size == 1) {
			#ifndef PERFORMANCE_TEST
			if (values.at(0) != PADDING_VALUE) {
				cout << (int) values.at(0) << endl;
			}
			#endif
		} else {
			sendValues(&values);
		}
	} else if (isLeaf) {
		receiveValue(root, &value);
	}

	#ifdef PERFORMANCE_TEST
	double start;
	double end;

	MPI_Barrier(MPI_COMM_WORLD);
	start = MPI_Wtime();
	#endif

	if (size > 1) {
		// NOTE: This may not be the correct approach (in which case I'm sorry if anyone is reading this)
		// but it just makes hell of a lot more sense than the 'for i=1 to 2n+(log n)-1' with occasional
		// 'do nothing' phase from lectures. It saves up on unnecessary communication (it could be even better)
		// and reduces synchronization issues.
		//
		// NOTE2: Also, why the HELL do we have to have a full-blown processor for a SINGLE value that does
		// nothing other than just play ping pong with it's parent?
		while (! quit) {
			if (isRoot && value != EMPTY_VALUE) {
				#ifndef PERFORMANCE_TEST
				if (value != PADDING_VALUE) {
					cout << value << endl;
				}
				#endif

				value = EMPTY_VALUE;
			} else if (! isLeaf && extract) {
				receiveValue(rightChild, &rightValue);
				receiveValue(leftChild, &leftValue);

				isRightEmpty = rightValue == EMPTY_VALUE;
				isLeftEmpty = leftValue == EMPTY_VALUE;

				if (isLeftEmpty && isRightEmpty) {
					// NOTE: It'd be better to terminate children immediately when we won't need
					// it anymore but it complicates both the algorithm and the communication
					// protocol. By terminating both children at the same time, when they both
					// send EMPTY_VALUE, allows us to keep communicating with both of them
					// without danger of deadlock and/or other complications.

					// Give a sock to Dobby
					rightValue = QUIT_VALUE;
					leftValue = QUIT_VALUE;
				} else if (! isLeftEmpty && (isRightEmpty || leftValue <= rightValue)) {
					// Not tested, but the <= is probably enough to make this sorting stable
					value = leftValue;
					leftValue = EMPTY_VALUE;
				} else if (! isRightEmpty && (isLeftEmpty || rightValue < leftValue)) {
					value = rightValue;
					rightValue = EMPTY_VALUE;
				}

				sendValue(rightChild, &rightValue);
				sendValue(leftChild, &leftValue);
			} else if (! isRoot && value != QUIT_VALUE) {
				// Keep pushing our value to our parent until he says stop
				sendValue(parent, &value);
				receiveValue(parent, &value);
			}

			if (isRoot) {
				// Root keeps switching between extracting a number and printing it
				quit = value == EMPTY_VALUE && leftValue == QUIT_VALUE && rightValue == QUIT_VALUE;
			} else {
				// Dobby has got a sock. Dobby is free!
				quit = value == QUIT_VALUE;
			}

			if (! isLeaf) {
				extract = value == EMPTY_VALUE && leftValue != QUIT_VALUE && rightValue != QUIT_VALUE;
			}
		}
	}

	#ifdef PERFORMANCE_TEST
	MPI_Barrier(MPI_COMM_WORLD);
	end = MPI_Wtime();
	#endif

	MPI_Finalize();

	#ifdef PERFORMANCE_TEST
	if (isRoot) {
		fprintf(stderr, "%.8f\n", end - start);
	}
	#endif

	return 0;
}
