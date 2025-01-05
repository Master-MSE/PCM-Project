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
#include <unistd.h>

#define _CRT_SECURE_NO_WARNINGS // evite les erreurs

#define DEFAULT_NUM_THREADS 2
#define MIN_WORK_SIZE 8

enum Verbosity {
    VER_NONE = 0,
    VER_GRAPH = 1,
    VER_SHORTER = 2,
    VER_BOUND = 4,
    VER_ANALYSE = 8,
    VER_COUNTERS = 16,
};

static const struct {
    char RED[6];
    char BLUE[6];
    char ORIGINAL[6];
} COLOR = {
        .RED = { 27, '[', '3', '1', 'm', 0 },
        .BLUE = { 27, '[', '3', '6', 'm', 0 },
        .ORIGINAL = { 27, '[', '3', '9', 'm', 0 },
};

struct ThreadData {
    Path* local_shortest;
    int thread_id;
};

static struct {
    Path* shortest;
    std::atomic<int> shortest_cost;
    Graph* graph;
    listcc<Path*> work_queue;
    Verbosity verbose;
    struct {
        std::atomic<int> verified;
        std::atomic<int> found;
        std::atomic<int>* bound;
    } counter;
} global;

static void branch_and_bound(Path* current, Path* shortest_local) {
    if (global.verbose & VER_ANALYSE)
        std::cout << "analysing " << current << '\n';

    if (current->leaf()) {
        current->add(0);

<<<<<<< HEAD
static void branch_and_bound(Path* current, Path* shortest_local_to_thread)
{
	if (global.verbose & VER_ANALYSE){
		//std::cout << "analysing " << cu<rrent << '\n';
	}
		
=======
        if (global.verbose & VER_COUNTERS)
            global.counter.verified++;
>>>>>>> 9f3b8f6 (improvement)

        if (shortest_local->distance() > current->distance()) {
            int current_shortest = global.shortest_cost.load();
            while (current_shortest > current->distance() &&
                   !global.shortest_cost.compare_exchange_weak(current_shortest, current->distance())) {}

            if (global.verbose & VER_SHORTER)
                std::cout << "shorter: " << current << '\n';

            shortest_local->copy(current);

<<<<<<< HEAD
		/*
				int current_shortest = global.shortest_cost.load();	
		while (current_shortest > current->distance() && 
				!global.shortest_cost.compare_exchange_weak(current_shortest, current->distance())){
			// Compare and set with retry for the shortest
		}
		*/


		if (global.verbose & VER_SHORTER)
			std::cout << "local shorter: " << current << '\n';
		shortest_local_to_thread->copy(current);
		
		if (global.verbose & VER_COUNTERS)
			global.counter.found ++;

		current->pop();
		
		return;
	} 


	if (current->distance() < global.shortest_cost.load()) {
		// continue branching
		for (int i=1; i<current->max(); i++) {
			if (!current->contains(i)) {
				current->add(i);
				branch_and_bound(current, shortest_local_to_thread);
				current->pop();
			}
		}
		return;
	}

	// current already >= shortest known so far, bound
	if (global.verbose & VER_BOUND )
		std::cout << "bound " << current << '\n';
	if (global.verbose & VER_COUNTERS)
		global.counter.bound[current->size()] ++;
	
	// remove to the total the factorial of the remaining paths not checked by the bound
	global.total.fetch_sub(global.fact[current->size()]);
	
=======
            if (global.verbose & VER_COUNTERS)
                global.counter.found++;
        }
        current->pop();
        return;
    }

    if (current->distance() >= global.shortest_cost.load()) {
        if (global.verbose & VER_BOUND)
            std::cout << "bound " << current << '\n';
        if (global.verbose & VER_COUNTERS)
            global.counter.bound[current->size()]++;
        return;
    }

    if (global.graph->size() - current->size() <= MIN_WORK_SIZE) {
        for (int i = 1; i < current->max(); i++) {
            if (!current->contains(i)) {
                current->add(i);
                branch_and_bound(current, shortest_local);
                current->pop();
            }
        }
        return;
    }

    for (int i = 1; i < current->max(); i++) {
        if (!current->contains(i)) {
            Path* new_path = new Path(global.graph);
            new_path->copy(current);
            new_path->add(i);
            global.work_queue.enqueue(new_path);
        }
    }
>>>>>>> 9f3b8f6 (improvement)
}

static void* thread_worker(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    Path* current = new Path(global.graph);

    while (true) {
        try {
            Path* work = global.work_queue.dequeue();
            current->copy(work);
            delete work;
            branch_and_bound(current, data->local_shortest);
        } catch (const std::runtime_error&) {
            break;
        }
    }

    if (data->local_shortest->distance() == global.shortest_cost.load()) {
        std::cout << "Shortest path found by thread " << data->thread_id << std::endl;
        global.shortest->copy(data->local_shortest);
    }

    delete current;
    delete data->local_shortest;
    delete data;
    return nullptr;
}

static void parallel_solve(int num_threads) {
    std::cout << "Starting " << num_threads << " threads..." << std::endl;

    Path* root = new Path(global.graph);
    root->add(0);

    for (int i = 1; i < global.graph->size(); i++) {
        Path* new_path = new Path(global.graph);
        new_path->copy(root);
        new_path->add(i);
        global.work_queue.enqueue(new_path);
    }
    delete root;

    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; i++) {
        ThreadData* data = new ThreadData;
        data->thread_id = i;
        data->local_shortest = new Path(global.graph);
        data->local_shortest->copy(global.shortest);

        int rc = pthread_create(&threads[i], nullptr, thread_worker, data);
        if (rc) {
            std::cout << "Error: unable to create thread " << rc << std::endl;
            exit(-1);
        }
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], nullptr);
    }
}

<<<<<<< HEAD
void *thread_routine(void *thread_id) {
	Path *local_shortest = new Path(global.graph);
	local_shortest->copy(global.shortest);
	Path *current;
	
	while (global.total > 0)
	{
		try {
			current = global.list.dequeue();
			// std::cout << global.total << std::endl;
		}
		catch(const std::exception& e) {
			// arbitrary sleep ?
			// std::cerr << e.what() << '\n';
			continue;
		}



		//while current
		bool continue_branching = true;
		
		while(continue_branching){
			continue_branching = false;

			if (global.graph->size()-current->size() <= SEQUENTIAL_THRESHOLD) {
			branch_and_bound(current, local_shortest);
			break;	
			}


			if (current->leaf()) {
				throw std::runtime_error("A thread should never hit a leaf !");
			} else {
				// not yet a leaf
				if (current->distance() < global.shortest->distance()) {
					// continue branching

					Path *new_current = new Path(global.graph);
					for (int i=1; i<current->max(); i++) {
						// if 1 put I in current
						// else create job
						if (!current->contains(i)) {
							// Unique global malloc ?
							Path *new_path = new Path(global.graph);
							new_path->copy(current);
							new_path->add(i);
						
							//if !newCurrent
							if(!continue_branching) {
								continue_branching = true;
								new_current->copy(new_path);
								//std::cout << "new Current for thread " << new_path << '\n';

							} else {
							//std::cout << "Adding new path to queue: " << new_path << '\n';
							global.list.enqueue(new_path);
							}
							
						}
					}
					if(continue_branching) {
						current->copy(new_current);
					} 
					//delete new_current;
				} else {
					// current already >= shortest known so far, bound
					if (global.verbose & VER_BOUND )
						//std::cout << "bound " << current << '\n';
					if (global.verbose & VER_COUNTERS)
						global.counter.bound[current->size()] ++;

					// remove to the total the factorial of the remaining paths not checked by the bound
					global.total.fetch_sub(global.fact[current->size()]);
				}
				
			}
		//end while
		}
		delete current;
	}

<<<<<<< HEAD
	if (global.shortest_cost.load() == local_shortest->distance()) {
=======
	

	if (global.shortest_cost < local_shortest->distance()) {
			int current_shortest = global.shortest_cost.load();	
		while (current_shortest > local_shortest->distance() && 
				!global.shortest_cost.compare_exchange_weak(current_shortest, local_shortest->distance())){
			// Compare and set with retry for the shortest
		}
>>>>>>> 0de0104 (test)
		std::cout << "Shortest path found by thread " << (long)thread_id << std::endl;
		global.shortest->copy(local_shortest);
	}
	
	delete local_shortest;


	std::cout << "Thread " << (long)thread_id << " finished" << std::endl;
	pthread_exit(NULL); 
=======
void solve_tsp(const char* fname, int num_threads) {
    global.graph = TSPFile::graph(fname);
    global.shortest = new Path(global.graph);

    if (global.verbose & VER_GRAPH)
        std::cout << COLOR.BLUE << global.graph << COLOR.ORIGINAL;

    global.counter.bound = new std::atomic<int>[global.graph->size()];
    for (int i = 0; i < global.graph->size(); i++) {
        global.counter.bound[i] = 0;
        global.shortest->add(i);
    }
    global.shortest->add(0);
    global.shortest_cost = global.shortest->distance();

    parallel_solve(num_threads);

    std::cout << COLOR.RED << "shortest " << global.shortest << COLOR.ORIGINAL << '\n';

    if (global.verbose & VER_COUNTERS) {
        std::cout << "verified: " << global.counter.verified << '\n';
        std::cout << "found shorter: " << global.counter.found << '\n';
        std::cout << "bound (per level):";
        for (int i = 0; i < global.graph->size(); i++)
            std::cout << ' ' << global.counter.bound[i];
        std::cout << '\n';
    }
>>>>>>> 9f3b8f6 (improvement)
}

int main(int argc, char* argv[]) {
    int num_threads = DEFAULT_NUM_THREADS;
    char* fname = nullptr;
    global.verbose = VER_NONE;

<<<<<<< HEAD
	pthread_t threads[num_threads];
	for (int i = 0; i < num_threads; i++) {
		int rc = pthread_create(&threads[i], NULL, thread_routine, (void *)i);
		if (rc) {
			std::cout << "Error:unable to create thread," << rc << std::endl;
         	exit(-1);
		}
	}
=======
    char c;
    while ((c = getopt(argc, argv, "v:t:f:")) != -1) {
        switch (c) {
            case 'v':
                global.verbose = (Verbosity)atoi(optarg);
                break;
            case 't':
                num_threads = atoi(optarg);
                break;
            case 'f':
                fname = optarg;
                break;
            default:
                fprintf(stderr, "usage: %s [-v#] [-t#] -f filename\n", argv[0]);
                exit(1);
        }
    }
>>>>>>> 9f3b8f6 (improvement)

    if (fname == nullptr) {
        fprintf(stderr, "usage: %s [-v#] [-t#] -f filename\n", argv[0]);
        exit(1);
    }

<<<<<<< HEAD
int main(int argc, char* argv[])
{
	int num_threads = DEFAULT_NUM_THREADS;

	char* fname = NULL;
	global.verbose = VER_NONE;
	
	char c;
	while ((c = getopt(argc, argv, "v:t:f:")) != -1)
	{
		switch (c)
		{
		case 'v':
			global.verbose = (Verbosity) atoi(optarg);
			break;
		
		case 't':
			num_threads = atoi(optarg);
			break;

		case 'f':
			fname = optarg;
			break;
			
		case '?':
			std::cout << "Got unknown option." << std::endl; 
			break;

		default:
			fprintf(stderr, "usage: %s [-v#] [-t#] -f filename", argv[0]);
			exit(1);
		}
	}

	if (fname == NULL) {
		fprintf(stderr, "usage: %s [-v#] [-t#] -f filename", argv[0]);
		exit(1);
	}
	

	global.graph = TSPFile::graph(fname);
	if (global.verbose & VER_GRAPH)
		std::cout << COLOR.BLUE << global.graph << COLOR.ORIGINAL;

	// if (global.verbose & VER_COUNTERS)
	reset_counters(global.graph->size());

	global.shortest = new Path(global.graph);
	for (int i=0; i<global.graph->size(); i++) {
		global.shortest->add(i);
	}
	global.shortest->add(0);
	global.shortest_cost = global.shortest->distance();

	Path* current = new Path(global.graph);
	current->add(0);
	global.list.enqueue(current);

	start_threads(num_threads);

	std::cout << COLOR.RED << "shortest " << global.shortest << COLOR.ORIGINAL << '\n';

	if (global.verbose & VER_COUNTERS)
		//print_counters();

	return 0;
}
=======
    solve_tsp(fname, num_threads);
    return 0;
}
>>>>>>> 9f3b8f6 (improvement)
