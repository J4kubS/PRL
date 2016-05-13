#include <iostream>
#include <fstream>
#include <vector>
#include <math.h>
#include <mpi.h>

#define HERE_BE_DRAGONS

// Uncomment to disable all stdout output and enable outputting
// run time of root (in seconds) to stderr.
//#define PERFORMANCE_TEST

using namespace std;

const string NUMBERS_FILE_NAME = "numbers";
const int FLAG_PROPAGATE = 1;
const int FLAG_GENERATE = 2;
const int FLAG_STOP = 3;
const int TAG_VALUE = 1;

inline void receive_value(int rank, int &value) {
	MPI_Recv(&value, 1, MPI_INT, rank, TAG_VALUE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

inline void send_value(int rank, int &value) {
	MPI_Send(&value, 1, MPI_INT, rank, TAG_VALUE, MPI_COMM_WORLD);
}

/*
 * Welcome to Fun with Flags!
 *
 * # | S | P | G
 * S | S | S | S
 * P | S | P | G
 * G | G | G | G
 *
*/
inline int flag_magic(int flag_1, int flag_2) {
	if (flag_1 == FLAG_PROPAGATE) {
		return flag_2;
	} else {
		return flag_1;
	}
}

inline int flag_init(int digit_1, int digit_2) {
	switch (digit_1 + digit_2) {
		case 2:
			return FLAG_GENERATE;
		case 1:
			return FLAG_PROPAGATE;
		default:
			return FLAG_STOP;
	}
}

inline void pad_number(vector<int> &number, int target) {
	for (int i = number.size(); i < target; i++) {
		number.insert(number.begin(), 0);
	}
}

void load_numbers(vector<int> &number_1, vector<int> &number_2) {
	vector<int> *number = &number_1;
	fstream input;
	int value;

	input.open(NUMBERS_FILE_NAME.c_str(), ios::in);

	while (input.good()) {
		value = input.get();

		// Skip EOF
		if (input.good()) {
			if (value != '\n') {
				number->push_back(value - '0');
			} else {
				number = &number_2;
			}
		}
	}

	input.close();
}

void send_numbers(vector<int> &number_1, vector<int> &number_2) {
	int size = number_1.size();

	for (int i = 0; i < size; i++) {
		int rank = size + i - 1;

		send_value(rank, number_1.at(i));
		send_value(rank, number_2.at(i));
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
	int parent = floor((rank - 1) / 2);
	int first_leaf = floor(size / 2);
	int r_child = 2 * rank + 2;
	int l_child = 2 * rank + 1;

	bool is_root = rank == root;
	bool is_node = ! is_root && rank < first_leaf;
	bool is_leaf = ! is_node && rank >= first_leaf;
	bool is_first_leaf = rank == first_leaf;
	bool is_last_leaf = rank == size - 1;

	int digit_1;
	int digit_2;
	int result;
	int carry;

	int i_flag;
	int l_flag;
	int r_flag;
	int flag;

	// Initialize
	if (is_root) {
		vector<int> number_1 = vector<int>();
		vector<int> number_2 = vector<int>();
		int target = (size + 1) / 2;

		load_numbers(number_1, number_2);
		pad_number(number_1, target);
		pad_number(number_2, target);

		// Edge case
		if (size == 1) {
			#ifndef PERFORMANCE_TEST
			digit_1 = number_1.at(0);
			digit_2 = number_2.at(0);

			cout << rank << ":" << ((digit_1 + digit_2) % 2) << endl;

			if (digit_1 == 1 && digit_2 == 1) {
				cout << "overflow" << endl;
			}
			#endif
		} else {
			send_numbers(number_1, number_2);
		}
	} else if (is_leaf) {
		receive_value(root, digit_1);
		receive_value(root, digit_2);

		// Calculate initial flag
		i_flag = flag_init(digit_1, digit_2);
	}

	#ifdef PERFORMANCE_TEST
	double start;
	double end;

	MPI_Barrier(MPI_COMM_WORLD);
	start = MPI_Wtime();
	#endif

	if (size > 1) {
		// Up-sweep
		if (! is_leaf) {
			receive_value(l_child, l_flag);
			receive_value(r_child, r_flag);

			flag = flag_magic(l_flag, r_flag);

			if (! is_root) {
				send_value(parent, flag);
			} else {
				// Result of reduce
				i_flag = flag;
			}
		} else {
			send_value(parent, i_flag);
		}

		// Down-sweep
		if (! is_leaf) {
			if (! is_root) {
				receive_value(parent, flag);
			} else {
				// Set root to the neutral value
				flag = FLAG_PROPAGATE;
			}

			l_flag = flag_magic(r_flag, flag);
			send_value(l_child, l_flag);
			send_value(r_child, flag);
		} else {
			receive_value(parent, flag);
		}

		// The following is required by the assignment to properly implement full flag-scan.
		//
		// 1) Shift flags in leaf processors in MSB -> LSB direction.
		//    Set flag at MSB position to the result of the up-sweep phase.
		//    At this point we have completed flag-scan.
		//
		// 2) Shift flags in leaf processors in LSB -> MSB direction.
		//    Set carry to 1 if and only if flag is 'generate', otherwise 0.
		//    Set carry of the LSB processor to 0 as it doesn't have carry input.
		#ifdef HERE_BE_DRAGONS
		// MSB -> LSB shift
		if (is_root) {
			send_value(first_leaf, i_flag);
		} else if (is_leaf) {
			if (! is_last_leaf) {
				send_value(rank + 1, flag);
			}

			if (! is_first_leaf) {
				receive_value(rank - 1, flag);
			} else {
				receive_value(root, flag);
			}
		}

		// LSB -> MSB shift
		if (is_leaf) {
			if (! is_first_leaf) {
				send_value(rank - 1, flag);
			}

			if (! is_last_leaf) {
				receive_value(rank + 1, flag);
			} else {
				// We don't care about carry for the last leaf (LSB adder)
				// because there is no adder to receive it from
				flag = FLAG_STOP;
			}

			if (flag == FLAG_GENERATE) {
				carry = 1;
			} else {
				carry = 0;
			}
		}
		// Or we could do it like this.
		#else
		if (is_leaf) {
			if (! is_last_leaf && flag == FLAG_GENERATE) {
				carry = 1;
			} else {
				carry = 0;
			}
		}
		#endif

		// Calculate value
		if (is_leaf) {
			result = (digit_1 + digit_2 + carry) % 2;

			#ifndef PERFORMANCE_TEST
			cout << rank << ":" << result << endl;
			#endif
		}
	}

	MPI_Barrier(MPI_COMM_WORLD);

	#ifdef PERFORMANCE_TEST
	end = MPI_Wtime();
	#else
	if (is_root && i_flag == FLAG_GENERATE) {
		cout << "overflow" << endl;
	}
	#endif

	MPI_Finalize();

	#ifdef PERFORMANCE_TEST
	if (is_root) {
		fprintf(stderr, "%.8f\n", end - start);
	}
	#endif

	return 0;
}
