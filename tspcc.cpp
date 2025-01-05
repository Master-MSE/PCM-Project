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
#include <vector>
#include <random>

#define _CRT_SECURE_NO_WARNINGS

#define DEFAULT_NUM_THREADS 2
#define SEQUENTIAL_THRESHOLD 8
#define LOCAL_WORK_STEAL_ATTEMPTS 3
#define WORK_STEAL_BATCH_SIZE 4

enum Verbosity {
    VER_NONE = 0,
    VER_GRAPH = 1,
    VER_SHORTER = 2,
    VER_BOUND = 4,
    VER_ANALYSE = 8,
    VER_COUNTERS = 16,
};

struct ThreadContext {
    Path* shortest_local;
    int shortest_cost_local;
    std::vector<Path*> local_queue;
    int thread_id;
    std::mt19937 rng;  // Générateur de nombres aléatoires local au thread
};

static struct {
    Path* shortest;
    std::atomic_int shortest_cost;
    Verbosity verbose;
    struct {
        std::atomic_int verified;
        std::atomic_int found;
        std::atomic_int* bound;
    } counter;
    int size;
    Graph *graph;
    std::atomic_uint64_t total;
    u_int64_t* fact;
    listcc<Path *> list;
    std::vector<ThreadContext*> thread_contexts;
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

bool steal_work(ThreadContext* context) {
    for (int attempts = 0; attempts < LOCAL_WORK_STEAL_ATTEMPTS; attempts++) {
        int victim = context->rng() % global.thread_contexts.size();
        if (victim == context->thread_id) continue;
        
        auto& victim_queue = global.thread_contexts[victim]->local_queue;
        
        // Essayer de voler plusieurs tâches d'un coup
        int steal_count = 0;
        for (int i = 0; i < WORK_STEAL_BATCH_SIZE && !victim_queue.empty(); ++i) {
            context->local_queue.push_back(victim_queue.back());
            victim_queue.pop_back();
            steal_count++;
        }
        
        if (steal_count > 0) return true;
    }
    return false;
}

static void branch_and_bound(Path* current, ThreadContext* context)
{
    if (current->leaf()) {
        current->add(0);
        if (global.verbose & VER_COUNTERS)
            global.counter.verified++;

        global.total--;

        // Vérification locale d'abord
        if (context->shortest_cost_local > current->distance()) {
            context->shortest_cost_local = current->distance();
            context->shortest_local->copy(current);
            
            // Mise à jour globale uniquement si nécessaire
            int global_shortest = global.shortest_cost.load(std::memory_order_relaxed);
            if (context->shortest_cost_local < global_shortest) {
                while (global_shortest > context->shortest_cost_local && 
                    !global.shortest_cost.compare_exchange_weak(global_shortest, 
                        context->shortest_cost_local, 
                        std::memory_order_release,
                        std::memory_order_relaxed)) {
                }
                
                if (global.verbose & VER_SHORTER)
                    std::cout << "Thread " << context->thread_id << " found shorter: " << current << '\n';
                
                if (global.verbose & VER_COUNTERS)
                    global.counter.found++;
            }
        }

        current->pop();
        return;
    }

    // Vérification par rapport au minimum local
    if (current->distance() < context->shortest_cost_local) {
        for (int i = current->max() - 1; i >= 1; i--) {
            if (!current->contains(i)) {
                current->add(i);
                branch_and_bound(current, context);
                current->pop();
            }
        }
        return;
    }

    // Bound case
    if (global.verbose & VER_BOUND)
        std::cout << "bound " << current << '\n';
    if (global.verbose & VER_COUNTERS)
        global.counter.bound[current->size()]++;
    
    global.total.fetch_sub(global.fact[current->size()], std::memory_order_relaxed);
}

void *thread_routine(void *arg) {
    ThreadContext* context = (ThreadContext*)arg;
    context->rng.seed(context->thread_id + time(nullptr));  // Initialisation du RNG
    
    while (global.total > 0) {
        Path* current = nullptr;
        
        // Essayer d'abord la queue locale
        if (!context->local_queue.empty()) {
            current = context->local_queue.back();
            context->local_queue.pop_back();
        } 
        // Ensuite essayer de voler du travail
        else if (!steal_work(context)) {
            // En dernier recours, utiliser la queue globale
            try {
                current = global.list.dequeue();
            } catch(const std::exception&) {
                if (global.total > 0) {
                    usleep(100);  // Court délai avant nouvelle tentative
                }
                continue;
            }
        }

        if (!current) continue;

        // Mise à jour périodique du shortest_cost_local
        if ((global.total.load(std::memory_order_relaxed) % 1000) == 0) {
            context->shortest_cost_local = std::min(
                context->shortest_cost_local,
                global.shortest_cost.load(std::memory_order_relaxed)
            );
        }

        if (global.graph->size() - current->size() <= SEQUENTIAL_THRESHOLD) {
            branch_and_bound(current, context);
            delete current;
            continue;
        }

        // Distribution du travail dans la queue locale
        if (current->distance() < context->shortest_cost_local) {
            for (int i = 1; i < current->max(); i++) {
                if (!current->contains(i)) {
                    Path *new_path = new Path(global.graph);
                    new_path->copy(current);
                    new_path->add(i);
                    context->local_queue.push_back(new_path);
                }
            }
        } else {
            // Bound case
            if (global.verbose & VER_BOUND)
                std::cout << "bound " << current << '\n';
            if (global.verbose & VER_COUNTERS)
                global.counter.bound[current->size()]++;
            
            global.total.fetch_sub(global.fact[current->size()], std::memory_order_relaxed);
        }
        
        delete current;
    }

    // Mise à jour finale du chemin le plus court si nécessaire
    if (context->shortest_cost_local == global.shortest_cost.load(std::memory_order_relaxed)) {
        global.shortest->copy(context->shortest_local);
    }

    std::cout << "Thread " << context->thread_id << " finished" << std::endl;
    pthread_exit(NULL);
}

void reset_counters(int size)
{
    global.size = size;
    global.counter.verified = 0;
    global.counter.found = 0;
    global.counter.bound = new std::atomic_int[size]();
    global.fact = new u_int64_t[size];
    
    for (int i = 0; i < size; i++) {
        global.counter.bound[i] = 0;
        if (i) {
            int pos = size - i;
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
    for (int i = 0; i < global.size; i++)
        std::cout << ' ' << global.counter.bound[i].load();
    std::cout << "\nbound equivalent (per level): ";
    u_int64_t equiv = 0;
    for (int i = 0; i < global.size; i++) {
        u_int64_t e = global.fact[i] * global.counter.bound[i].load();
        std::cout << ' ' << e;
        equiv += e;
    }
    std::cout << "\nbound equivalent (total): " << equiv << '\n';
    std::cout << "check: total " << (global.total == (global.counter.verified + equiv) ? "==" : "!=")
              << " verified + total bound equivalent\n";
}

void start_threads(int num_threads) {
    std::cout << "Starting " << num_threads << " threads..." << std::endl;

    // Initialisation des contextes de thread
    global.thread_contexts.reserve(num_threads);
    for (int i = 0; i < num_threads; i++) {
        ThreadContext* context = new ThreadContext();
        context->shortest_local = new Path(global.graph);
        context->shortest_local->copy(global.shortest);
        context->shortest_cost_local = global.shortest_cost.load();
        context->thread_id = i;
        global.thread_contexts.push_back(context);
    }

    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; i++) {
        int rc = pthread_create(&threads[i], NULL, thread_routine, (void *)global.thread_contexts[i]);
        if (rc) {
            std::cout << "Error:unable to create thread," << rc << std::endl;
            exit(-1);
        }
    }

    for (int i = 0; i < num_threads; i++) {
        int rc = pthread_join(threads[i], NULL);
        if (rc) {
            std::cout << "Error:unable to join thread," << rc << std::endl;
            exit(-1);
        }
    }

    // Nettoyage
    for (auto context : global.thread_contexts) {
        delete context->shortest_local;
        delete context;
    }
}

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

    reset_counters(global.graph->size());

    global.shortest = new Path(global.graph);
    for (int i = 0; i < global.graph->size(); i++) {
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

    // Nettoyage final
    delete global.shortest;
    delete[] global.counter.bound;
    delete[] global.fact;
    delete global.graph;

    return 0;
}