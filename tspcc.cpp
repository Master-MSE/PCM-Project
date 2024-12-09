//
//  tspcc.cpp
//  
//  Copyright (c) 2022 Marcelo Pasin. All rights reserved.
//

#include "graph.hpp"
#include "path.hpp"
#include "tspfile.hpp"
#include "listcc.hpp"
#include <pthread.h>

#define _CRT_SECURE_NO_WARNINGS // evite les erreurs

#define NUM_THREADS 5
#define SEQUENTIAL_THRESHOLD 8

enum Verbosity {
	VER_NONE = 0,
	VER_GRAPH = 1,
	VER_SHORTER = 2,
	VER_BOUND = 4,
	VER_ANALYSE = 8,
	VER_COUNTERS = 16,
};

static struct {
	Path* shortest;
	Verbosity verbose;
	struct {
		int verified;	// # of paths checked
		int found;	// # of times a shorter path was found
		int* bound;	// # of bound operations per level
	} counter;
	int size;
	int graph_size;
	Graph *graph;
	int total;		// number of paths to check
	int* fact;
	listcc<Path *> list;
} global;

static const struct {
	char RED[6];
	char BLUE[6];
	char ORIGINAL[6];
} COLOR = {
	.RED = { 27, '[', '3', '1', 'm', 0 },
	.BLUE = { 27, '[', '3', '6', 'm', 0 },
	.ORIGINAL = { 27, '[', '3', '9', 'm', 0 },
};


static void branch_and_bound(Path* current)
{
	if (global.verbose & VER_ANALYSE)
		std::cout << "analysing " << current << '\n';

	if (current->leaf()) {
		// this is a leaf
		current->add(0);
		if (global.verbose & VER_COUNTERS)
			global.counter.verified ++;
		if (current->distance() < global.shortest->distance()) {
			if (global.verbose & VER_SHORTER)
				std::cout << "shorter: " << current << '\n';
			global.shortest->copy(current);
			if (global.verbose & VER_COUNTERS)
				global.counter.found ++;
		}
		current->pop();
	} else {
		// not yet a leaf
		if (current->distance() < global.shortest->distance()) {
			// continue branching
			for (int i=1; i<current->max(); i++) {
				if (!current->contains(i)) {
					current->add(i);
					branch_and_bound(current);
					current->pop();
				}
			}
		} else {
			// current already >= shortest known so far, bound
			if (global.verbose & VER_BOUND )
				std::cout << "bound " << current << '\n';
			if (global.verbose & VER_COUNTERS)
				global.counter.bound[current->size()] ++;
		}
	}
}


void reset_counters(int size)
{
	global.size = size;
	global.counter.verified = 0;
	global.counter.found = 0;
	global.counter.bound = new int[global.size];
	global.fact = new int[global.size];
	for (int i=0; i<global.size; i++) {
		global.counter.bound[i] = 0;
		if (i) {
			int pos = global.size - i;
			global.fact[pos] = (i-1) ? (i * global.fact[pos+1]) : 1;
		}
	}
	global.total = global.fact[0] = global.fact[1];
}

void print_counters()
{
	std::cout << "total: " << global.total << '\n';
	std::cout << "verified: " << global.counter.verified << '\n';
	std::cout << "found shorter: " << global.counter.found << '\n';
	std::cout << "bound (per level):";
	for (int i=0; i<global.size; i++)
		std::cout << ' ' << global.counter.bound[i];
	std::cout << "\nbound equivalent (per level): ";
	int equiv = 0;
	for (int i=0; i<global.size; i++) {
		int e = global.fact[i] * global.counter.bound[i];
		std::cout << ' ' << e;
		equiv += e;
	}
	std::cout << "\nbound equivalent (total): " << equiv << '\n';
	std::cout << "check: total " << (global.total==(global.counter.verified + equiv) ? "==" : "!=") << " verified + total bound equivalent\n";
}

void *thread_routine(void *thread_id) {
	while (true)
	{
		Path *current;
		try {
			current = global.list.dequeue();
		}
		catch(const std::exception& e) {
			pthread_exit(NULL);
			std::cerr << e.what() << '\n';
			continue;
		}

		if (global.graph_size-current->size() <= SEQUENTIAL_THRESHOLD) {
			branch_and_bound(current);
			continue;		
		}

		if (current->leaf()) {
			current->add(0);
			if (global.verbose & VER_COUNTERS)
				global.counter.verified ++;

				// TODO Rendre Atomique :
			if (current->distance() < global.shortest->distance()) {				
				if (global.verbose & VER_SHORTER)
					std::cout << "shorter: " << current << '\n';
				global.shortest->copy(current);
				if (global.verbose & VER_COUNTERS)
					global.counter.found ++;
			}
			current->pop();
		} else {
			// not yet a leaf
			if (current->distance() < global.shortest->distance()) {
				// continue branching
				for (int i=1; i<current->max(); i++) {
					if (!current->contains(i)) {
						Path *new_path = new Path(global.graph);
						new_path->copy(current);
						new_path->add(i);
						global.list.enqueue(new_path);
					}
				}
			} else {
				// current already >= shortest known so far, bound
				if (global.verbose & VER_BOUND )
					std::cout << "bound " << current << '\n';
				if (global.verbose & VER_COUNTERS)
					global.counter.bound[current->size()] ++;
			}
		}
	}

	std::cout << "Thread " << (long)thread_id << " finished" << std::endl;
	pthread_exit(NULL); 
}

void start_threads() {
	std::cout << "Starting " << NUM_THREADS << " threads..." << std::endl;

	pthread_t threads[NUM_THREADS];
	for (int i = 0; i < NUM_THREADS; i++) {
		int rc = pthread_create(&threads[i], NULL, thread_routine, (void *)i);
		if (rc) {
			std::cout << "Error:unable to create thread," << rc << std::endl;
         	exit(-1);
		}
	}

	for (int i = 0; i < NUM_THREADS; i++) {
		int rc = pthread_join(threads[i], NULL);
	
		if (rc) {
			std::cout << "Error:unable to join thread," << rc << std::endl;
         	exit(-1);
		}
	}
}

int main(int argc, char* argv[])
{
	char* fname = 0;
	if (argc == 2) {
		fname = argv[1];
		global.verbose = VER_NONE;
	} else {
		if (argc == 3 && argv[1][0] == '-' && argv[1][1] == 'v') {
			global.verbose = (Verbosity) (argv[1][2] ? atoi(argv[1]+2) : 1);
			fname = argv[2];
		} else {
			fprintf(stderr, "usage: %s [-v#] filename\n", argv[0]);
			exit(1);
		}
	}

	global.graph = TSPFile::graph(fname);
	global.graph_size = global.graph->size();
	if (global.verbose & VER_GRAPH)
		std::cout << COLOR.BLUE << global.graph << COLOR.ORIGINAL;

	if (global.verbose & VER_COUNTERS)
		reset_counters(global.graph->size());

	global.shortest = new Path(global.graph);
	for (int i=0; i<global.graph->size(); i++) {
		global.shortest->add(i);
	}
	global.shortest->add(0);

	Path* current = new Path(global.graph);
	current->add(0);
	global.list.enqueue(current);

	start_threads();

	std::cout << COLOR.RED << "shortest " << global.shortest << COLOR.ORIGINAL << '\n';

	if (global.verbose & VER_COUNTERS)
		print_counters();

	return 0;
}
