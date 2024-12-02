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
class listcc
{
private:
    template <typename U>
    struct Node
	{
		U value;
		atomic_stamped<Node<U>*> nextref;
		Node(U v): nextref(0,0), value(v) {}
	};

    atomic_stamped<Node<T>> headref;
	atomic_stamped<Node<T>> tailref;
public:

    void printList() {
        uint64_t stamp;
        Node<T> *current = headref.get(stamp);
        
        int i = 0;
        while(current) {
            uint64_t nstamp;
            Node<T> *next = current->nextref.get(nstamp);

            std::cout << i << " : " << current->value;
            current = next;
        }
    }

    void enqueue(T value)
	{
		Node<T> *node = new Node<T>(value);
		int tailStamp, nextStamp, stamp;

		while (true) {
			Node<T> *tail = tailref.get(tailStamp);
			Node<T> *next = tail.nextref.get(nextStamp);
			if (tail == tailref.get(stamp) && stamp == tailStamp) {
				if (next == NULL) {
					if (tail.nextref.cas(next, node, nextStamp, nextStamp+1)) {
						tailref.cas(tail, node, tailStamp, tailStamp+1);
						return;
					}
				} else {
					tailref.cas(tail, next, tailStamp, tailStamp+1);
				}
			}
		}
	}

    T dequeue()
	{
		int tailStamp, headStamp, nextStamp, stamp;

		while (true) {
			Node<T> *head = headref.get(headStamp);
			Node<T> *tail = tailref.get(tailStamp);
			Node<T> *next = head->nextref.get(nextStamp);
			if (head == headref.get(stamp) && stamp == headStamp) {
				if (head == tail) {
					if (next == NULL)
						return NULL;
					tailref.cas(tail, next, tailStamp, tailStamp+1);
				} else {
					T value = next.value;
					if (headref.cas(head, next, headStamp, headStamp+1)) {
						delete head;
						return value;
                    }
				}
			}
		}
	}

    listcc() {
        Node<T> *node = new Node<T>(0);
		headref.set(node, 0);
        tailref.set(node, 0);
    };
    ~listcc() {
        uint64_t stamp;
        Node<T> *current = headref.get(stamp);
        
        while(current) {
            uint64_t nstamp;
            Node<T> *next = current->nextref.get(nstamp);

            delete current;
            current = next;
        }
        
    };
};

#endif