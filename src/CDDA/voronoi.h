#ifndef H_VORONOI_H
#define H_VORONOI_H

#include <inttypes.h>

typedef double (*VorDistFn)(double x, double y);
#define VORDIST_NAME(name) vordist_##name
#define VORDIST_FUNC(name) double VORDIST_NAME(name)(double x, double y)

typedef struct vor_site{
	typedef int32_t coord_t;
	coord_t x, y;
} vor_site;

#endif