#include<phase2.h>
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

typedef struct procStruct procStruct;
typedef struct procStruct * procPtr;
struct procStruct {
        int pid;
        procPtr					nextSleepPtr;
        procPtr					nextProcPtr;
        int     		        privateMBoxID;
        int						WakeTime;
		// Needed to make requests
		int 		opr;
		// void*		reg1;
		// void*		reg2;
		int 		track;
		int 		first;
		int 		sectors;
		void*		buffer;
        int             mboxTermDriver;
        int             mboxTermReal;
};

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
int diskSizeReal(int, int*, int*, int*);
int termReadReal(int, int, char *);
int termWriteReal(int, int, char *);
void initializeProcTable();
void setUserMode();
void updateProcTable(int);
static void check_kernel_mode(char *);
void addToSleepList(int , procPtr *,long );
int diskDoWork(procPtr, int );
int diskRequest(int, int, int, int, void*);
void addToList(int, procPtr*, int);
void printProcList(procPtr*);

#endif /* _PHASE4_H */
