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
#include <atomic>
#include <stdexcept>

#define _CRT_SECURE_NO_WARNINGS
#define DEFAULT_NUM_THREADS 2
#define SEQUENTIAL_THRESHOLD 8
#define CACHE_LINE_SIZE 64
#define LOCAL_QUEUE_CAPACITY 1024

enum Verbosity {
    VER_NONE = 0,
    VER_GRAPH = 1,
    VER_SHORTER = 2,
    VER_BOUND = 4,
    VER_ANALYSE = 8,
    VER_COUNTERS = 16,
};

struct alignas(CACHE_LINE_SIZE) ThreadContext {
    Path* shortest_local;
    std::atomic<int> shortest_cost_local;
    std::vector<Path*> local_queue;
    int thread_id;
    char padding[CACHE_LINE_SIZE - sizeof(Path*) - sizeof(std::atomic<int>) - sizeof(std::vector<Path*>) - sizeof(int)];

    ThreadContext() : shortest_local(nullptr), shortest_cost_local(INT_MAX), thread_id(-1) {
        local_queue.reserve(LOCAL_QUEUE_CAPACITY);
    }
};

static struct {
    Path* shortest;
    std::atomic<int> shortest_cost;
    Verbosity verbose;
    struct {
        std::atomic<int> verified;
        std::atomic<int> found;
        std::atomic<int>* bound;
    } counter;
    int size;
    Graph* graph;
    std::atomic<uint64_t> total;
    uint64_t* fact;
    listcc<Path*> list;
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
    if (!context) return false;

    const int max_attempts = 3;
    const int batch_size = 4;

    for (int attempt = 0; attempt < max_attempts; attempt++) {
        // Tenter de voler des threads voisins d'abord
        int left = (context->thread_id - 1 + global.thread_contexts.size()) % global.thread_contexts.size();
        int right = (context->thread_id + 1) % global.thread_contexts.size();

        for (int victim_id : {left, right}) {
            auto victim = global.thread_contexts[victim_id];
            if (!victim || victim == context) continue;

            auto& victim_queue = victim->local_queue;
            if (!victim_queue.empty()) {
                int steal_count = std::min(batch_size, (int)victim_queue.size());
                for (int i = 0; i < steal_count; i++) {
                    Path* stolen = victim_queue.back();
                    if (stolen) {
                        context->local_queue.push_back(stolen);
                        victim_queue.pop_back();
                    }
                }
                if (!context->local_queue.empty()) return true;
            }
        }
    }
    return false;
}

static void branch_and_bound(Path* current, ThreadContext* context) {
    if (!current || !context) return;

    try {
        if (current->leaf()) {
            if (current->size() + 1 >= global.graph->size()) {
                current->add(0);
                if (global.verbose & VER_COUNTERS) {
                    global.counter.verified++;
                }

                global.total--;

                int current_distance = current->distance();
                int local_best = context->shortest_cost_local.load(std::memory_order_relaxed);

                if (current_distance < local_best) {
                    context->shortest_cost_local.store(current_distance, std::memory_order_relaxed);
                    context->shortest_local->copy(current);

                    int global_best = global.shortest_cost.load(std::memory_order_relaxed);
                    while (current_distance < global_best &&
                           !global.shortest_cost.compare_exchange_weak(global_best, current_distance,
                                                                     std::memory_order_release,
                                                                     std::memory_order_relaxed)) {}

                    if (global.verbose & VER_SHORTER) {
                        std::cout << "Thread " << context->thread_id << " found shorter: " << current << '\n';
                    }
                }

                current->pop();
            }
            return;
        }

        // Vérification de la taille
        if (current->size() >= global.graph->size()) {
            return;
        }

        int local_best = context->shortest_cost_local.load(std::memory_order_relaxed);
        if (current->distance() < local_best) {
            for (int i = current->max() - 1; i >= 1; i--) {
                if (!current->contains(i)) {
                    current->add(i);
                    branch_and_bound(current, context);
                    current->pop();
                }
            }
        } else {
            if (global.verbose & VER_BOUND) {
                std::cout << "bound " << current << '\n';
            }
            if (global.verbose & VER_COUNTERS && current->size() < global.size) {
                global.counter.bound[current->size()]++;
            }
            global.total.fetch_sub(global.fact[current->size()], std::memory_order_relaxed);
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in branch_and_bound: " << e.what() << std::endl;
    }
}

void* thread_routine(void* arg) {
    ThreadContext* context = static_cast<ThreadContext*>(arg);
    if (!context) {
        std::cerr << "Null context in thread routine!" << std::endl;
        pthread_exit(NULL);
    }

    try {
        while (global.total > 0) {
            Path* current = nullptr;

            if (!context->local_queue.empty()) {
                current = context->local_queue.back();
                context->local_queue.pop_back();
            } else if (!steal_work(context)) {
                try {
                    current = global.list.dequeue();
                } catch (const std::exception&) {
                    if (global.total > 0) {
                        usleep(100);
                    }
                    continue;
                }
            }

            if (!current) continue;

            try {
                // Mise à jour périodique du coût local
                if ((global.total.load(std::memory_order_relaxed) % 1000) == 0) {
                    int global_cost = global.shortest_cost.load(std::memory_order_relaxed);
                    int local_cost = context->shortest_cost_local.load(std::memory_order_relaxed);
                    if (global_cost < local_cost) {
                        context->shortest_cost_local.store(global_cost, std::memory_order_relaxed);
                    }
                }

                if (global.graph->size() - current->size() <= SEQUENTIAL_THRESHOLD) {
                    branch_and_bound(current, context);
                    delete current;
                    continue;
                }

                if (current->size() < global.graph->size() - 1) {
                    int local_best = context->shortest_cost_local.load(std::memory_order_relaxed);
                    if (current->distance() < local_best) {
                        bool work_distributed = false;
                        for (int i = 1; i < current->max(); i++) {
                            if (!current->contains(i)) {
                                Path* new_path = new Path(global.graph);
                                new_path->copy(current);
                                new_path->add(i);
                                context->local_queue.push_back(new_path);
                                work_distributed = true;
                            }
                        }

                        if (!work_distributed) {
                            branch_and_bound(current, context);
                        }
                    } else {
                        if (global.verbose & VER_BOUND) {
                            std::cout << "bound " << current << '\n';
                        }
                        if (global.verbose & VER_COUNTERS && current->size() < global.size) {
                            global.counter.bound[current->size()]++;
                        }
                        global.total.fetch_sub(global.fact[current->size()], std::memory_order_relaxed);
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Exception in thread processing: " << e.what() << std::endl;
            }

            delete current;
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal exception in thread " << context->thread_id << ": " << e.what() << std::endl;
    }

    if (context->shortest_cost_local.load(std::memory_order_relaxed) == 
        global.shortest_cost.load(std::memory_order_relaxed)) {
        global.shortest->copy(context->shortest_local);
    }

    pthread_exit(NULL);
}

void reset_counters(int size) {
    if (size <= 0) {
        throw std::invalid_argument("Invalid size in reset_counters");
    }

    global.size = size;
    global.counter.verified = 0;
    global.counter.found = 0;
    global.counter.bound = new std::atomic<int>[size]();
    global.fact = new uint64_t[size];

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

void print_counters() {
    std::cout << "total: " << global.total << '\n';
    std::cout << "verified: " << global.counter.verified << '\n';
    std::cout << "found shorter: " << global.counter.found << '\n';
    std::cout << "bound (per level):";
    for (int i = 0; i < global.size; i++) {
        std::cout << ' ' << global.counter.bound[i].load();
    }
    std::cout << "\nbound equivalent (per level): ";
    uint64_t equiv = 0;
    for (int i = 0; i < global.size; i++) {
        uint64_t e = global.fact[i] * global.counter.bound[i].load();
        std::cout << ' ' << e;
        equiv += e;
    }
    std::cout << "\nbound equivalent (total): " << equiv << '\n';
    std::cout << "check: total " << (global.total == (global.counter.verified + equiv) ? "==" : "!=")
              << " verified + total bound equivalent\n";
}

void distribute_initial_work(int num_threads) {
    try {
        Path* initial_path = new Path(global.graph);
        initial_path->add(0);
        
        // Créer les premiers chemins
        std::vector<Path*> initial_paths;
        for (int i = 1; i < global.graph->size(); i++) {
            Path* new_path = new Path(global.graph);
            new_path->copy(initial_path);
            new_path->add(i);
            initial_paths.push_back(new_path);
        }
        
        // Distribution aux threads
        int paths_per_thread = initial_paths.size() / num_threads;
        int remainder = initial_paths.size() % num_threads;
        
        size_t path_index = 0;
        for (int i = 0; i < num_threads && path_index < initial_paths.size(); i++) {
            int count = paths_per_thread + (i < remainder ? 1 : 0);
            for (int j = 0; j < count && path_index < initial_paths.size(); j++) {
                global.thread_contexts[i]->local_queue.push_back(initial_paths[path_index++]);
            }
        }
        
        delete initial_path;
    } catch (const std::exception& e) {
        std::cerr << "Exception in distribute_initial_work: " << e.what() << std::endl;
        throw;
    }
}

void start_threads(int num_threads) {
    std::cout << "Starting " << num_threads << " threads..." << std::endl;

    try {
        // Initialisation des contextes
        global.thread_contexts.reserve(num_threads);
        for (int i = 0; i < num_threads; i++) {
            ThreadContext* context = new ThreadContext();
            context->shortest_local = new Path(global.graph);
            context->shortest_local->copy(global.shortest);
            context->shortest_cost_local.store(global.shortest_cost.load());
            context->thread_id = i;
            global.thread_contexts.push_back(context);
        }

        // Distribution initiale du travail
        distribute_initial_work(num_threads);

        // Création des threads
        std::vector<pthread_t> threads(num_threads);
        for (int i = 0; i < num_threads; i++) {
            int rc = pthread_create(&threads[i], NULL, thread_routine, global.thread_contexts[i]);
            if (rc) {
                throw std::runtime_error("Failed to create thread: " + std::to_string(rc));
            }
        }

        // Attente des threads
        for (int i = 0; i < num_threads; i++) {
            int rc = pthread_join(threads[i], NULL);
            if (rc) {
                throw std::runtime_error("Failed to join thread: " + std::to_string(rc));
            }
        }

        // Nettoyage
        for (auto context : global.thread_contexts) {
            delete context->shortest_local;
            delete context;
        }
        global.thread_contexts.clear();

    } catch (const std::exception& e) {
        std::cerr << "Exception in start_threads: " << e.what() << std::endl;
        throw;
    }
}

int main(int argc, char* argv[]) {
    try {
        int num_threads = DEFAULT_NUM_THREADS;
        char* fname = NULL;
        global.