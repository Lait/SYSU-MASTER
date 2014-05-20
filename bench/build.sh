rm bench
mpicc  -o bench src/*.c   -fopenmp -lev
