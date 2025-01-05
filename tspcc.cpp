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
#include <memory>
#include <vector>

#define _CRT_SECURE_NO_WARNINGS

#define DEFAULT_NUM_THREADS 2
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
    Path* shortest;  // Revert to raw pointer because of operator<< requirements
    std::atomic_int shortest_cost;
    Verbosity verbose;
    struct {
        int verified;
        int found;
        std::unique_ptr<int[]> bound;
    } counter;
    int size;
    Graph* graph;  // Revert to raw pointer because of operator<< requirements
    std::atomic_uint64_t total;
    std::unique_ptr<u_int64_t[]> fact;
    listcc<Path*> list;
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

static void branch_and_bound(Path* current, Path* shortest_local_to_thread)
{
    if (global.verbose & VER_ANALYSE)
        std::cout << "analysing " << current << '\n';

    if (current->leaf()) {
        current->add(0);
        if (global.verbose & VER_COUNTERS)
            global.counter.verified++;

        global.total--;

        if (shortest_local_to_thread->distance() <= current->distance()) {
            current->pop();
            return;
        }

        int current_shortest = global.shortest_cost.load();    
        while (current_shortest > current->distance() && 
                !global.shortest_cost.compare_exchange_weak(current_shortest, current->distance())) {
        }

        if (global.verbose & VER_SHORTER)
            std::cout << "local shorter: " << current << '\n';
        shortest_local_to_thread->copy(current);
        
        if (global.verbose & VER_COUNTERS)
            global.counter.found++;

        current->pop();
        
        return;
    } 

    if (current->distance() < global.shortest_cost.load()) {
        for (int i=1; i<current->max(); i++) {
            if (!current->contains(i)) {
                current->add(i);
                branch_and_bound(current, shortest_local_to_thread);
                current->pop();
            }
        }
        return;
    }

    if (global.verbose & VER_BOUND)
        std::cout << "bound " << current << '\n';
    if (global.verbose & VER_COUNTERS)
        global.counter.bound[current->size()]++;
    
    global.total.fetch_sub(global.fact[current->size()]);
}

void reset_counters(int size)
{
    global.size = size;
    global.counter.verified = 0;
    global.counter.found = 0;
    global.counter.bound = std::make_unique<int[]>(global.size);
    global.fact = std::make_unique<u_int64_t[]>(global.size);
    
    for (int i=0; i<global.size; i++) {
        global.counter.bound[i] = 0;
        if (i) {
            int pos = global.size - i;
            global.fact[pos] = (i-1) ? (i * global.fact[pos+1]) : 1;
        }
    }
    global.fact[0] = global.fact[1];
    global.total = global.fact[0];
}

void print_counters()
{
    std::cout << "total: " << global.total << '\n';
    std::cout << "verified: " << global.counter.verified << '\n';
    std::cout << "found shorter: " << global.counter.found << '\n';
    std::cout << "bound (per level):";
    for (u_int64_t i=0; i<global.size; i++)
        std::cout << ' ' << global.counter.bound[i];
    std::cout << "\nbound equivalent (per level): ";
    u_int64_t equiv = 0;
    for (u_int64_t i=0; i<global.size; i++) {
        u_int64_t e = global.fact[i] * global.counter.bound[i];
        std::cout << ' ' << e;
        equiv += e;
    }
    std::cout << "\nbound equivalent (total): " << equiv << '\n';
    std::cout << "check: total " << (global.total==(global.counter.verified + equiv) ? "==" : "!=") << " verified + total bound equivalent\n";
}

void cleanup() {
    delete global.shortest;
    delete global.graph;
}

void *thread_routine(void *thread_id) {
    Path *local_shortest = new Path(global.graph);
    local_shortest->copy(global.shortest);
    Path *current;
    
    while (global.total > 0) {
        try {
            current = global.list.dequeue();
        }
        catch(const std::exception& e) {
            continue;
        }

        if (!current) continue;

        if (global.graph->size()-current->size() <= SEQUENTIAL_THRESHOLD) {
            branch_and_bound(current, local_shortest);
            delete current;
            continue;        
        }

        if (current->leaf()) {
            delete current;
            throw std::runtime_error("A thread should never hit a leaf !");
        } else {
            if (current->distance() < global.shortest->distance()) {
                for (int i=1; i<current->max(); i++) {
                    if (!current->contains(i)) {
                        Path* new_path = new Path(global.graph);
                        new_path->copy(current);
                        new_path->add(i);
                        global.list.enqueue(new_path);
                    }
                }
            } else {
                if (global.verbose & VER_BOUND)
                    std::cout << "bound " << current << '\n';
                if (global.verbose & VER_COUNTERS)
                    global.counter.bound[current->size()]++;

                global.total.fetch_sub(global.fact[current->size()]);
            }
            delete current;
        }
    }

    if (global.shortest_cost.load() == local_shortest->distance()) {
        std::cout << "Shortest path found by thread " << (long)thread_id << std::endl;
        global.shortest->copy(local_shortest);
    }

    delete local_shortest;
    std::cout << "Thread " << (long)thread_id << " finished" << std::endl;
    pthread_exit(NULL); 
}

void start_threads(int num_threads) {
    std::cout << "Starting " << num_threads << " threads..." << std::endl;

    // Use raw array instead of vector for pthread_t
    pthread_t* threads = new pthread_t[num_threads];
    
    for (int i = 0; i < num_threads; i++) {
        int rc = pthread_create(&threads[i], NULL, thread_routine, (void *)static_cast<intptr_t>(i));
        if (rc) {
            std::cout << "Error:unable to create thread," << rc << std::endl;
            delete[] threads;
            exit(-1);
        }
    }

    for (int i = 0; i < num_threads; i++) {
        int rc = pthread_join(threads[i], NULL);
        if (rc) {
            std::cout << "Error:unable to join thread," << rc << std::endl;
            delete[] threads;
            exit(-1);
        }
    }
    
    delete[] threads;
}

int main(int argc, char* argv[])
{
    int num_threads = DEFAULT_NUM_THREADS;
    char* fname = NULL;
    global.verbose = VER_NONE;
    
    char c;
    while ((c = getopt(argc, argv, "v:t:f:")) != -1) {
        switch (c) {
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

    reset_counters(global.graph->size());

    global.shortest = new Path(global.graph);
    for (int i=0; i<global.graph->size(); i++) {
        global.shortest->add(i);
    }
    global.shortest->add(0);
    global.shortest_cost = global.shortest->distance();

    Path* initial_path = new Path(global.graph);
    initial_path->add(0);
    global.list.enqueue(initial_path);

    start_threads(num_threads);

    std::cout << COLOR.RED << "shortest " << global.shortest << COLOR.ORIGINAL << '\n';

    if (global.verbose & VER_COUNTERS)
        print_counters();
        
    cleanup();
    return 0;
}