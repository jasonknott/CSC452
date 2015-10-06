/* -------------------Libraries----------------- */
#include <stdio.h>
#include <stdlib.h>
#include "phase1.h"
#include "phase2.h"
#include <string.h>
#define DEBUG2 1
/* --------------------------------------------- */
/* -------------------Prototypes---------------- */
void nullsys(systemArgs *);
void clockHandler2(int, void*);
void diskHandler(int, void*);
void termHandler(int, void*);
void syscallHandler(int, void*);
/* --------------------------------------------- */