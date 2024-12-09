#  Copyright (c) 2012 Marcelo Pasin. All rights reserved.

CFLAGS=-O3 -Wall
LDFLAGS=-latomic

all: tspcc atomic

tspcc: tspcc.o
	clang++ -o tspcc $(LDFLAGS) tspcc.o

tspcc.o: tspcc.cpp graph.hpp path.hpp tspfile.hpp listcc.hpp atomic.hpp
	clang++ $(CFLAGS) -c tspcc.cpp
	
omp:
	make tspcc CFLAGS="-fopenmp -O3" LDFLAGS="-fopenmp -O3"

clean:
	rm -f *.o tspcc atomic
