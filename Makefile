#  Copyright (c) 2012 Marcelo Pasin. All rights reserved.

CFLAGS=-O3 -Wall
LDFLAGS=-O3 -lm

all: tspcc atomic

tspcc: tspcc.o
	c++ -o tspcc $(LDFLAGS) tspcc.o

tspcc.o: tspcc.cpp graph.hpp path.hpp tspfile.hpp
	c++ $(CFLAGS) -c tspcc.cpp

atomic: atomic.cpp
	clang++ -o atomic -latomic $(LDFLAGS) atomic.cpp

omp:
	make tspcc CFLAGS="-fopenmp -O3" LDFLAGS="-fopenmp -O3"

clean:
	rm -f *.o tspcc atomic
