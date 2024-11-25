//
//  path.hpp
//  
//  Copyright (c) 2022 Marcelo Pasin. All rights reserved.
//

#include <iostream>

#ifndef _path_hpp
#define _path_hpp

class Path {
private:
	int _size;
	int _distance;
	int* _nodes;
	Graph* _graph;
public:
	~Path()
	{
		clear();
		delete[] _nodes;
		_nodes = 0;
		_graph = 0;
	}

	Path(Graph* graph)
	{
		_graph = graph;
		_nodes = new int[max() + 1];
		_distance = 0;
		clear();
	}

	int max() const { return _graph->size(); }
	int size() const { return _size; }
	bool leaf() const { return (_size == max()); }
	int distance() const { return _distance; }
	void clear() { _size = _distance = 0; }

	void add(int node)
	{
		if (_size <= max()) {
			if (_size) {
				int last = _nodes[_size - 1];
				int distance = _graph->distance(last, node);
				_distance += distance;
			}
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
		}
	}

	bool contains(int node) const
	{
		for (int i=0; i<_size; i++)
			if (_nodes[i] == node)
				return true;
		return false;
	}

	void copy(Path* o)
	{
		if (max() != o->max()) {
			delete[] _nodes;
			_nodes = new int[o->max() + 1];
		}
		_graph = o->_graph;
		_size = o->_size;
		_distance = o->_distance;
		for (int i=0; i<_size; i++)
			_nodes[i] = o->_nodes[i];
	}

	void print(std::ostream& os) const
	{
		os << '[' << _distance;
		for (int i=0; i<_size; i++)
			os << (i?',':':') << ' ' << _nodes[i];
		os << ']';
	}
};

std::ostream& operator <<(std::ostream& os, Path* p)
{
	p->print(os);
	return os;
}

#endif // _path_hpp