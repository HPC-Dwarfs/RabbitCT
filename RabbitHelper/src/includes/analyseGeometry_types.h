#ifndef ANALYSEGEOMETRY_TYPES_H
#define ANALYSEGEOMETRY_TYPES_H

#include <stdint.h>

typedef struct {
    int x;
    int y;
    int z;
} Point3D;

typedef struct {
    int Umin;
    int Umax;
    int Vmin;
    int Vmax;
} OutShadow;

typedef struct {
    int16_t start;
    int16_t stop;
} LineRange;



#endif /*ANALYSEGEOMETRY_TYPES_H*/
