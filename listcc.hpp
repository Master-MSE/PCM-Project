/*

    listcc.hpp
    author : Alec Schmidt & Loic Fontaine
    date   : 2024
*/

#ifndef _listcc_hpp
#define _listcc_hpp

#include "graph.hpp"
#include "path.hpp"
#include "atomic.hpp"
#include <iostream>

template <typename T>
class listcc {
private:
    struct Node {
        T value;
        atomic_stamped<Node> nextref;
		 Node(T val) : value(val), nextref(nullptr, 0) {}
    };

    atomic_stamped<Node> headref;
    atomic_stamped<Node> tailref;

public:
    listcc() : headref(nullptr, 0), tailref(nullptr, 0) {
        // Initialiser les références atomiques avec un Node fictif
        Node* dummy = new Node(T()); // Créer un nœud fictif pour initialisation
        headref.set(dummy, 0);
        tailref.set(dummy, 0);
    }

    ~listcc() {
        uint64_t stamp;
        Node* current = headref.get(stamp);
        while (current != nullptr) {
            Node* next = current->nextref.get(stamp);
            delete current;
            current = next;
        }
    }

    void enqueue(T value) {
        Node* newNode = new Node(value);

        uint64_t stamp, nextStamp;
        while (true) {
            Node* tail = tailref.get(stamp);
            Node* next = tail->nextref.get(nextStamp);

            if (tail == tailref.get(stamp)) {
                if (next == nullptr) {
                    if (tail->nextref.cas(next, newNode, nextStamp, nextStamp + 1)) {
                        tailref.cas(tail, newNode, stamp, stamp + 1);
                        return;
                    }
                } else {
                    tailref.cas(tail, next, stamp, stamp + 1);
                }
            }
        }
    }

    T dequeue() {
        uint64_t headStamp, tailStamp, nextStamp;
        while (true) {
            Node* head = headref.get(headStamp);
            Node* tail = tailref.get(tailStamp);
            Node* next = head->nextref.get(nextStamp);

            if (head == headref.get(headStamp)) {
                if (head == tail) {
                    if (next == nullptr) {
                        throw std::runtime_error("Queue is empty");
                    }
                    tailref.cas(tail, next, tailStamp, tailStamp + 1);
                } else {
                    T value = next->value;
                    if (headref.cas(head, next, headStamp, headStamp + 1)) {
                        delete head;
                        return value;
                    }
                }
            }
        }
    }
	 // Afficher le contenu de la liste
    void printList() {
        uint64_t stamp;
        listcc::Node* current = headref.get(stamp)->nextref.get(stamp); // Sauter le nœud fictif
        int index = 0;

        while (current != nullptr) {
            std::cout << index << " : " << current->value << std::endl;
            current = current->nextref.get(stamp);
            index++;
        }
    }
};

#endif