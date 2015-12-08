
#include "usloss.h"
#define DEBUG 0
#include "phase5.h"
extern int debugflag5;


void
p1_fork(int pid)
{
    if (DEBUG && debugflag5)
        USLOSS_Console("p1_fork() called: pid = %d\n", pid);

    if (vmStarted == FALSE){
    	return;
    }

} /* p1_fork */

void
p1_switch(int old, int new)
{
    if (DEBUG && debugflag5)
        USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);
    
    if (vmStarted == FALSE){
    	return;
    }

    vmStats.switches++;
	// if oldpid is not  > 0 
 //    check processes old pid > 0 
/*
    go through page table for old pid
    	if old framenum != 0
    		unmap(page for old)
    if new processes pid >  0:
    	go through page table for new 
    		if old pagetablke[i].framenum != -1
    			map(page which is i, frame, RW)
*/

} /* p1_switch */

void
p1_quit(int pid)
{
    if (DEBUG && debugflag5)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);

    if (vmStarted == FALSE){
    	return;
    }
/*
    for(pagetable )
    	if pagetable[i].framenum != -1:
    		unmap(page/i, frame)
    		frametable[framenum].status = unused
    		frametable[framenum].pid = -1
    		vmStats.freeFrames++;
    for(diskblockTable (????)):
    	if[diskblockTable[i].pid == pid (the pid calling quit)]:
    		diskblockTable[i].status = unused;
  
    procTable[pid].pid = -1
*/

} /* p1_quit */

