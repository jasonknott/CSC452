
#include "usloss.h"
#define DEBUG 0
#include "phase5.h"
extern int debugflag5;


void
p1_fork(int pid)
{
    if (DEBUG && debugflag5)
        USLOSS_Console("p1_fork() called: pid = %d\n", pid);
} /* p1_fork */

void
p1_switch(int old, int new)
{
    if (DEBUG && debugflag5)
        USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);
    vmStats.switches++;
} /* p1_switch */

void
p1_quit(int pid)
{
    if (DEBUG && debugflag5)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);
} /* p1_quit */

