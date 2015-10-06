/* -------------------Libraries----------------- */
#include <stdio.h>
#include <stdlib.h>
#include "phase1.h"
#include "phase2.h"
#include <string.h>
#include "message.h"
/* --------------------------------------------- */
/* -------------------Prototypes---------------- */
void nullsys(sysargs *);
void clockHandler2(int, void*);
void diskHandler(int, void*);
void termHandler(int, void*);
void syscallHandler(int, void*);
/* --------------------------------------------- */