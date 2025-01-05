#include "graph.hpp"
#include "path.hpp"
#include "tspfile.hpp"
#include "listcc.hpp"
#include <pthread.h>
#include <unistd.h>

// Définition de constantes et désactivation des avertissements de sécurité pour certains outils
#define _CRT_SECURE_NO_WARNINGS

#define DEFAULT_NUM_THREADS 2    // Nombre de threads par défaut
#define MIN_WORK_SIZE 8         // Taille minimale de travail pour diviser le travail

// Enumération pour la gestion de la verbosité des sorties
enum Verbosity {
    VER_NONE = 0,             // Aucune sortie
    VER_GRAPH = 1,            // Informations sur le graphe
    VER_SHORTER = 2,          // Chemins plus courts trouvés
    VER_BOUND = 4,            // Bornes atteintes
    VER_ANALYSE = 8,          // Analyse détaillée
    VER_COUNTERS = 16,        // Compteurs
};

// Définition des couleurs pour la sortie console (pour une meilleure lisibilité)
static const struct {
    char RED[6];
    char BLUE[6];
    char ORIGINAL[6];
} COLOR = {
        .RED = { 27, '[', '3', '1', 'm', 0 },
        .BLUE = { 27, '[', '3', '6', 'm', 0 },
        .ORIGINAL = { 27, '[', '3', '9', 'm', 0 },
};

// Structure contenant des informations sur chaque thread (ID et le plus court chemin local)
struct ThreadData {
    Path* local_shortest;
    int thread_id;
};

// Variables globales utilisées dans le programme
static struct {
    Path* shortest;                   // Le plus court chemin trouvé globalement
    std::atomic<int> shortest_cost;    // Coût du plus court chemin
    Graph* graph;                      // Le graphe représentant le problème TSP
    listcc<Path*> work_queue;          // File d'attente des travaux pour les threads
    Verbosity verbose;                 // Niveau de verbosité des sorties
    struct {
        std::atomic<int> verified;     // Nombre de vérifications
        std::atomic<int> found;        // Nombre de chemins plus courts trouvés
        std::atomic<int>* bound;       // Compteur pour les bornes
    } counter;
} global;

// Fonction principale de l'algorithme branch-and-bound
static void branch_and_bound(Path* current, Path* shortest_local) {
    // Analyse du chemin actuel (si le niveau de verbosité le permet)
    if (global.verbose & VER_ANALYSE)
        std::cout << "analysing " << current << '\n';

    // Si le chemin est une feuille (solution complète)
    if (current->leaf()) {
<<<<<<< HEAD
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
=======
        current->add(0);  // Retour au point de départ
        global.counter.verified++;  // Incrémentation du compteur de vérifications
>>>>>>> 3eb16bc (comments)

        // Mise à jour du plus court chemin si nécessaire
        if (shortest_local->distance() > current->distance()) {
            int current_shortest = global.shortest_cost.load();
            while (current_shortest > current->distance() &&
                   !global.shortest_cost.compare_exchange_weak(current_shortest, current->distance())) {}

            // Affichage si le chemin trouvé est plus court
            if (global.verbose & VER_SHORTER)
                std::cout << "shorter: " << current << '\n';

<<<<<<< HEAD
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
=======
            shortest_local->copy(current);  // Copie du nouveau chemin le plus court
            global.counter.found++;  // Incrémentation du compteur de chemins trouvés
>>>>>>> 3eb16bc (comments)
        }

        current->pop();  // Retour en arrière pour explorer d'autres chemins
        return;
    }

    // Si le coût du chemin actuel est supérieur au coût du plus court chemin connu
    if (current->distance() >= global.shortest_cost.load()) {
        if (global.verbose & VER_BOUND)
            std::cout << "bound " << current << '\n';  // Affichage si la borne est atteinte
        if (global.verbose & VER_COUNTERS)
            global.counter.bound[current->size()]++;  // Compteur pour les bornes
        return;
    }

    // Si le travail restant est suffisamment petit, exploration exhaustive
    if (global.graph->size() - current->size() <= MIN_WORK_SIZE) {
        for (int i = 1; i < current->max(); i++) {
            if (!current->contains(i)) {
                current->add(i);
                branch_and_bound(current, shortest_local);  // Exploration du sous-problème
                current->pop();  // Retour en arrière
            }
        }
        return;
    }

    // Si le travail restant est plus grand, division du travail entre les threads
    for (int i = 1; i < current->max(); i++) {
        if (!current->contains(i)) {
            Path* new_path = new Path(global.graph);
            new_path->copy(current);
            new_path->add(i);
            global.work_queue.enqueue(new_path);  // Ajout à la file de travail
        }
    }
>>>>>>> 9f3b8f6 (improvement)
}

// Fonction exécutée par chaque thread
static void* thread_worker(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    Path* current = new Path(global.graph);

    // Boucle principale du thread pour traiter les travaux
    while (true) {
        try {
            Path* work = global.work_queue.dequeue();  // Récupère un travail
            current->copy(work);  // Copie le travail dans le chemin local
            delete work;  // Libère l'objet travail
            branch_and_bound(current, data->local_shortest);  // Résolution du sous-problème
        } catch (const std::runtime_error&) {
            break;  // Si la file de travail est vide, le thread termine
        }
    }

    // Si ce thread a trouvé le plus court chemin
    if (data->local_shortest->distance() == global.shortest_cost.load()) {
        std::cout << "Shortest path found by thread " << data->thread_id << std::endl;
        global.shortest->copy(data->local_shortest);  // Mise à jour du chemin global
    }

    delete current;
    delete data->local_shortest;
    delete data;
    return nullptr;
}

// Fonction pour résoudre le problème en parallèle
static void parallel_solve(int num_threads) {
    std::cout << "Starting " << num_threads << " threads..." << std::endl;

    // Création et initialisation du chemin de départ
    Path* root = new Path(global.graph);
    root->add(0);  // Ajout du premier noeud (le point de départ)

    // Ajout des chemins de départ dans la file de travail
    for (int i = 1; i < global.graph->size(); i++) {
        Path* new_path = new Path(global.graph);
        new_path->copy(root);
        new_path->add(i);
        global.work_queue.enqueue(new_path);  // Enfile le travail
    }
    delete root;

    // Création et démarrage des threads
    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; i++) {
        ThreadData* data = new ThreadData;
        data->thread_id = i;
        data->local_shortest = new Path(global.graph);
        data->local_shortest->copy(global.shortest);

        int rc = pthread_create(&threads[i], nullptr, thread_worker, data);  // Création du thread
        if (rc) {
            std::cout << "Error: unable to create thread " << rc << std::endl;
            exit(-1);
        }
    }

    // Attente de la fin de tous les threads
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], nullptr);
    }
}

<<<<<<< HEAD
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
=======
// Fonction principale pour résoudre le problème du TSP
>>>>>>> 3eb16bc (comments)
void solve_tsp(const char* fname, int num_threads) {
    global.graph = TSPFile::graph(fname);  // Chargement du graphe depuis un fichier
    global.shortest = new Path(global.graph);  // Création d'un chemin pour le résultat final

    // Affichage du graphe si le niveau de verbosité le permet
    if (global.verbose & VER_GRAPH)
        std::cout << COLOR.BLUE << global.graph << COLOR.ORIGINAL;

    global.counter.bound = new std::atomic<int>[global.graph->size()];
    for (int i = 0; i < global.graph->size(); i++) {
        global.counter.bound[i] = 0;
        global.shortest->add(i);  // Ajout d'un point de départ
    }
    global.shortest->add(0);  // Retour au point de départ
    global.shortest_cost = global.shortest->distance();  // Coût du plus court chemin

    parallel_solve(num_threads);  // Résolution parallèle du problème

    // Affichage du résultat final
    std::cout << COLOR.RED << "shortest " << global.shortest << COLOR.ORIGINAL << '\n';

    // Affichage des statistiques si la verbosité le permet
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

// Fonction principale du programme
int main(int argc, char* argv[]) {
    // Traitement des arguments de ligne de commande
    int num_threads = DEFAULT_NUM_THREADS;
<<<<<<< HEAD
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
=======
    if (argc > 1) {
        num_threads = std::stoi(argv[1]);  // Nombre de threads
    }
    const char* fname = argv[2];  // Nom du fichier à traiter

    solve_tsp(fname, num_threads);  // Résolution du problème
}
>>>>>>> 3eb16bc (comments)
