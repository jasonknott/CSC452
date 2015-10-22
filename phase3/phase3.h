/*
 * These are the definitions for phase 3 of the project
 */

#ifndef _PHASE3_H
#define _PHASE3_H

#define MAXSEMS         200
#define DEBUG3 1

#endif /* _PHASE3_H */

 typedef struct procStruct procStruct;
 typedef struct procStruct * procPtr;

 struct procStruct {
 	int pid;
 	int priority;
 	char name[MAXNAME];
 	char            startArg[MAXARG];
 	int (* start_func) (char *);
 	unsigned int    stackSize;
 	procPtr         nextProcPtr;
 	procPtr         childProcPtr;
 	procPtr         nextSiblingPtr;
 	int				privateMBoxID;
    int             parentPid;
 };

