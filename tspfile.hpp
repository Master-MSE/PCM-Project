//
//  tspfile.hpp
//
//  Copyright (c) 2022 Marcelo Pasin. All rights reserved.
//

#ifndef  _tspfile_hpp
#define  _tspfile_hpp

#include <math.h>
#include <cerrno>
#include <cstring>

#include "graph.hpp"

// quand PI n'est par défaut dans c++
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


class TSPFile {
private:
	static const int MAX_NODES = 100;
	static const int MAX_CHARS_LINE = 1000;

	enum Weight { EWT_EUC_2D = 1, EWT_GEO, EWT_ERR };
	struct Point { double x, y; };
	static int _linenum;
	static std::string _filename;

	static void abort(std::string str, int err = 0)
	{
		if (_linenum)
			std::cerr << "Line " << _linenum << " in " << _filename << ": ";
		std::cerr << str;
		if (err)
			std::cerr << '(' << std::strerror(err) << ')';
		std::cerr << '\n';
		exit(1);
	}

	static int sqdist(double x0, double y0, double x1, double y1)
	{
		x0 -= x1;
		y0 -= y1;
		return (int) (.5 + sqrt(x0*x0 + y0*y0));
	}

	static int lldist(double lo0, double la0, double lo1, double la1)
	{
		double RRR = 6378.388;
		la0 = la0 * M_PI / 180.;
		lo0 = lo0 * M_PI / 180.;
		la1 = la1 * M_PI / 180.;
		lo1 = lo1 * M_PI / 180.;
		double q1 = cos(lo0 - lo1);
		double q2 = cos(la0 - la1);
		double q3 = cos(la0 + la1);
		return (int) (RRR * acos( ( (q1+1)*q2 - (q1-1)*q3 ) /2 ) + .5);
	}

	static int scan_size(char* line)
	{
		int size = 0;
		line = trim_line(line, true);
		sscanf(line, "%d", &size);
		if (size > MAX_NODES)
			abort("too many points in input");
		if (size < 1)
			abort("wrong size in input");
		return size;		
	}

	static Weight scan_weight(char* line)
	{
		line = trim_line(line, true);
		if (!strncmp("EUC_2D", line, 6)) {
			return EWT_EUC_2D;
		} else if (!strncmp("GEO", line, 3)) {
			return EWT_GEO;
		}
		return EWT_ERR;
	}

	static Point scan_point(char* line, int i)
	{
		Point point;
		int j;
		if (sscanf(line, "%d %lf %lf", &j, &point.x, &point.y) != 3)
			abort("missing data in input file");
		if (i != (j-1))
			abort("wrong data in input file");
		return point;
	}

	static char* trim_line(char* line, bool search_colon = false)
	{
		char* head = line;

		if (search_colon) {
			head = strchr(head, ':');
			if (!head)
				abort("missing colon");
			head ++;
		}
		while (*head && isspace(*head))
			head ++;
		char* tail = head + strlen(head) - 1;
		while (tail >= head && isspace(*tail))
			*tail-- = 0;
		return head;
	}

public:
	static Graph* graph(std::string fname)
	{
		FILE *f;
		int size = 0;
		char line[MAX_CHARS_LINE];
		char* tline;
		Point vec[MAX_NODES];
		Weight ewt = EWT_EUC_2D;
	
		_linenum = 0;
		_filename = fname;

		f = fopen(fname.c_str(), "r");
		if (!f)
			abort(fname.c_str(), errno);

		while (1) {
			fgets(line, MAX_CHARS_LINE-1, f);
			tline = trim_line(line);
			_linenum ++;
			if (!strncmp("DIMENSION", tline, 9)) {
				size = scan_size(tline);
			} else if (!strncmp("EDGE_WEIGHT_TYPE", tline, 16)) {
				ewt = scan_weight(tline);
			} else if (!strncmp("NODE_COORD_SECTION", line, 18))
				break;
			if (feof(f))
				abort(fname.c_str(), errno);
		}
		for (int i=0; i<size; i++) {
			fgets(line, MAX_CHARS_LINE-1, f);
			tline = trim_line(line);
			_linenum ++;
			vec[i] = scan_point(tline, i);
		}
		fclose(f);

		Graph* g = new Graph(size);
		for (int i=0; i<size; i++) {
			g->add(vec[i].x, vec[i].y);
			g->sdistance(i, i) = 0;
			for (int j=0; j<i; j++) {
				int dist = 0;
				switch (ewt) {
					case EWT_GEO:
						dist = lldist(vec[i].x, vec[i].y, vec[j].x, vec[j].y);
						break;
					case EWT_EUC_2D:
						dist = sqdist(vec[i].x, vec[i].y, vec[j].x, vec[j].y);
						break;
					case EWT_ERR:
						abort("wrong EDGE_WEIGHT_TYPE parameter");
				}
				g->sdistance(j, i) = g->sdistance(i, j) = dist;
			}
		}

		return g;
	}

};

int TSPFile::_linenum;
std::string TSPFile::_filename;

#endif //  _tspfile_hpp
