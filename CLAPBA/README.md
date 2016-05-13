# CLAPBA
Implementation of Carry Look Ahead Parallel Binary Adder using [Open MPI](https://www.open-mpi.org/) library.

## How to run
```
$ ./test.sh <number of bits>
```
Program will read two binary numbers (represented by `0` and `1` ASCII characters) from the `numbers` file. Each number must be on a separate line.

## Example
```
$ cat numbers
1010
1101

$ ./test.sh 4
overflow
3:0
4:1
5:1
6:1
```

