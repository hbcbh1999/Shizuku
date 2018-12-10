#pragma once
#include <stdio.h>

#define MAXOBSTS 20

#ifdef REDUCED_RESOLUTION
#define MAX_XDIM 256
#define MAX_YDIM 256
#else
#define MAX_XDIM 768
#define MAX_YDIM 768
#endif

#define INITIAL_UMAX 0.125f
#define BLOCKSIZEX 64
#define BLOCKSIZEY 1
#define LINE_OBST_WIDTH 1
#define PI 3.141592653589793238463
#define SMAG_CONST 1.f

enum ContourVariable{VEL_MAG,VEL_U,VEL_V,PRESSURE,STRAIN_RATE,WATER_RENDERING};
enum ViewMode{TWO_DIMENSIONAL,THREE_DIMENSIONAL};