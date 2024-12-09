//
//  path.hpp
//  
//  Copyright (c) 2022 Marcelo Pasin. All rights reserved.
//

#include <iostream>

#ifndef _path_hpp
#define _path_hpp

#include "graph.hpp"

class Path {
private:
	const static int MAX = 32;
	int _size;
	int _distance;
	char _nodes[MAX];
	int _bitmap;
	Graph* _graph;
public:
	~Path()
	{
		clear();
		_graph = 0;
	}

	Path(Graph* graph)
	{
		_graph = graph;
		clear();
	}

	int max() const { return _graph->size(); }
	int size() const { return _size; }
	bool leaf() const { return (_size == max()); }
	int distance() const { return _distance; }
	void clear() { _size = _distance = _bitmap = 0; }

	void add(int node)
	{
		if (_size <= max()) {
			if (_size) {
				int last = _nodes[_size - 1];
				int distance = _graph->distance(last, node);
				_distance += distance;
			}
			_bitmap |= (1<<node);
			_nodes[_size ++] = node;
		}
	}

	void pop()
	{
		if (_size) {
			int last = _nodes[-- _size];
			if (_size) {
				int node = _nodes[_size - 1];
				int distance = _graph->distance(node, last);
				_distance -= distance;
			}
			_bitmap &= ~(1<<last);
		}
	}

	bool contains(int node) const
	{
		return _bitmap & (1<<node);
	}

	void copy(Path* o)
	{
		_graph = o->_graph;
		_size = o->_size;
		_distance = o->_distance;
		_bitmap = o->_bitmap;
		for (int i=0; i<_size; i++)
			_nodes[i] = o->_nodes[i];
	}

	void print(std::ostream& os) const
	{
		os << '[' << _distance;
		for (int i=0; i<_size; i++)
			os << (i?',':':') << ' ' << (int)_nodes[i];
		os << ']';
	}
};

std::ostream& operator <<(std::ostream& os, Path* p)
{
	p->print(os);
	return os;
}

#endif // _path_hpp