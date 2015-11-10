/*
 * These are the definitions for phase 4 of the project (support level, part 2).
 */

#ifndef _PHASE4_H
#define _PHASE4_H

/*
 * Maximum line length
 */
#define MAXLINE         80
#define ERR_INVALID     -1
#define ERR_OK           0


#define DEBUG4 1
/*
 * Function prototypes for this phase.
 */

extern  int  Sleep(int seconds);
extern  int  DiskRead (void *diskBuffer, int unit, int track, int first, 
                       int sectors, int *status);
extern  int  DiskWrite(void *diskBuffer, int unit, int track, int first,
                       int sectors, int *status);
extern  int  DiskSize (int unit, int *sector, int *track, int *disk);
extern  int  TermRead (char *buffer, int bufferSize, int unitID,
                       int *numCharsRead);
extern  int  TermWrite(char *buffer, int bufferSize, int unitID,
                       int *numCharsRead);

extern  int  start4(char *);

void sleep(systemArgs *);
void diskRead(systemArgs *);
void diskWrite(systemArgs *);
void diskSize(systemArgs *);
void termRead(systemArgs *);
void termWrite(systemArgs *);
int sleepReal(int);
int diskReadReal(int, int, int, int, void *);
int diskWriteReal(int, int, int, int, void *);
int diskSizeReal(int, int, int, int);
int termReadReal(int, int, char *);
int termWriteReal(int, int, char *);

typedef struct procStruct procStruct;
typedef struct procStruct * procPtr;
struct procStruct {
        int pid;
        // int priority;
        // char name[MAXNAME];
        // char            startArg[MAXARG];
        // int (* start_func) (char *);
        // unsigned int    stackSize;
        procPtr         nextSleepPtr;
        // procPtr         childSleepPtr;
        // procPtr         nextSiblingPtr;
        int             privateMBoxID;
        int				WakeTime;
	    // int             parentPid;
    	// int             started;
 };


#endif /* _PHASE4_H */
