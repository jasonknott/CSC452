#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <stdlib.h> /* needed for atoi() */
#include <stdio.h>
#include <phase3.h>
#include <phase4.h>
#include <provided_prototypes.h>
int 	running;

static int	ClockDriver(char *);
static int	DiskDriver(char *);

/* the process table */
procStruct ProcTable[MAXPROC];
procPtr sleepList;



int debugflag4 = 0;

void start3(void){
    char	name[128];
    char    termbuf[10];
    char    buf[5];
    int		i;
    int		clockPID;
    int		pid;
    int		status;

    if(debugflag4 && DEBUG4)
        USLOSS_Console("start3(): started\n");
    /*
     * Check kernel mode here.
     */
    check_kernel_mode("start3");

    /*
     * Initialize stuff
     */
    initializeProcTable();
    sleepList = NULL;

    /*
     * Add things to systemCallVec
     */
    systemCallVec[SYS_SLEEP] = (void *)sleep;
    systemCallVec[SYS_DISKREAD] = (void *)diskRead;
    systemCallVec[SYS_DISKWRITE] = (void *)diskWrite;
    systemCallVec[SYS_DISKSIZE] = (void *)diskSize;
    systemCallVec[SYS_TERMREAD] = (void *)termRead;
    systemCallVec[SYS_TERMWRITE] = (void *)termWrite;

    /*
     * Create clock device driver 
     * I am assuming a semaphore here for coordination.  A mailbox can
     * be used instead -- your choice.
     */
    running = semcreateReal(0);

    clockPID = fork1("Clock driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2);
    if (clockPID < 0) {
    	USLOSS_Console("start3(): Can't create clock driver\n");
    	USLOSS_Halt(1);
    }
    /*
     * Wait for the clock driver to start. The idea is that ClockDriver
     * will V the semaphore "running" once it is running.
     */

    sempReal(running);

    // Adding clock proc to table
    ProcTable[clockPID%MAXPROC].pid = clockPID;

    /*
     * Create the disk device drivers here.  You may need to increase
     * the stack size depending on the complexity of your
     * driver, and perhaps do something with the pid returned.
     */

    if(debugflag4 && DEBUG4)
        USLOSS_Console("start3(): about to fork diskSize\n");

    for (i = 0; i < USLOSS_DISK_UNITS; i++) {
        sprintf(buf, "%d", i);
        pid = fork1(name, DiskDriver, buf, USLOSS_MIN_STACK, 2);
        // Maybe storing the pid's into an array so you can clean them up later
        if (pid < 0) {
            USLOSS_Console("start3(): Can't create disk driver %d\n", i);
            USLOSS_Halt(1);
        }
        // Adding disk Proc drivers to Proc table
        ProcTable[pid%MAXPROC].pid = pid;
        USLOSS_Console("adding %i to ProcTable\n", pid);
    }
    int temp = pid;

    if(debugflag4 && DEBUG4)
        USLOSS_Console("start3(): Finished forking diskSize\n");

    sempReal(running);
    sempReal(running);

    if(debugflag4 && DEBUG4)
        USLOSS_Console("start3(): Pass 3rd sempReal\n");

    /*
     * Create terminal device drivers.
     */
     // Remember to store these pid's as well so you can clean them up after
     // Also remember to add these to proc table


    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * I'm assuming kernel-mode versions of the system calls
     * with lower-case first letters.
     */


    pid = spawnReal("start4", start4, NULL, 4 * USLOSS_MIN_STACK, 3);
    pid = waitReal(&status);

    /*
     * Zap the device drivers
     */
    USLOSS_Console("Trying to zap %i\n", clockPID);
    zap(clockPID);  // clock driver
    // Zap everything else
    for (int i = 0; i < MAXPROC; ++i){
        if (ProcTable[i % MAXPROC].pid != -1){
            USLOSS_Console("Trying to zap %i\n", i);
            zap(i);
        }
    }

    // eventually, at the end:
    quit(0);
}

static int ClockDriver(char *arg){
    if(debugflag4 && DEBUG4)
        USLOSS_Console("ClockDriver(): started\n");
    int result;
    int status;

    // Let the parent know we are running and enable interrupts.
    semvReal(running);
    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);

    // Infinite loop until we are zap'd
    while(! isZapped()) {
    	result = waitDevice(USLOSS_CLOCK_DEV, 0, &status);
    	if (result != 0) {
    	    return 0;
        }

    	/*
    	 * Compute the current time and wake up any processes
    	 * whose time has come.
    	 */
        procPtr proc = sleepList;
        int currentTime = USLOSS_Clock();
        while(proc != NULL) {
            if(proc->WakeTime <= currentTime){
            // Send to free a process
            MboxSend(ProcTable[proc->pid % MAXPROC].privateMBoxID, 0, 0);
            sleepList = proc;
            }
            proc= proc->nextSleepPtr;
        }
    }
    procPtr proc = sleepList;
    // LET ME PEOPLE GO!
    while(proc != NULL){
        // Send to free a process
        MboxSend(ProcTable[proc->pid % MAXPROC].privateMBoxID, 0, 0);
    }

    quit(0); //I think...
    return 0;
}

static int DiskDriver(char *arg){
    if(debugflag4 && DEBUG4)
        USLOSS_Console("DiskDriver(): started\n");
    int unit = atoi( (char *) arg); 	// Unit is passed as arg.
    int result;
    int status;


    semvReal(running); //maybe?

    while(! isZapped()) {
        // if(debugflag4 && DEBUG4)
        //     USLOSS_Console("DiskDriver(): Running in loop\n");
        result = waitDevice(USLOSS_CLOCK_DEV, 0, &status);
        if (result != 0) {
            return 0;
        }

    }

    return 0;
}


//The following parses syscalls:
void sleep(systemArgs * args){
    int seconds = (long) args->arg1;
    int returnValue = sleepReal(seconds);
    args->arg4 = (void *)((long)returnValue);
    setUserMode();
}

void diskRead(systemArgs * args)
{
    void * buff = args->arg1;
    int sectors = (long) args->arg2;
    int track = (long) args->arg3;
    int first = (long) args->arg4;
    int unit = (long) args->arg5;
    int returnValue = diskReadReal(unit, track, first, sectors, buff);
    if(returnValue == -1)
        args->arg4 = (void*)((long)-1);
    else
        args->arg4 = (void*)((long)0);
    if(returnValue != -1)
        args->arg1 = (void*)((long)returnValue);
}

void diskWrite(systemArgs * args)
{
    void * buff = args->arg1;
    int sectors = (long) args->arg2;
    int track = (long) args->arg3;
    int first = (long) args->arg4;
    int unit = (long) args->arg5;
    int returnValue = diskReadReal(unit, track, first, sectors, buff);
    if(returnValue == -1)
        args->arg4 = (void*)((long)-1);
    else
        args->arg4 = (void*)((long)0);
    if(returnValue != -1)
        args->arg1 = (void*)((long)returnValue);
}

void diskSize(systemArgs * args)
{
    int unit = (long) args->arg1;
    int sector = 0;
    int track = 0;
    int disk = 0;
    diskSizeReal(unit, &sector, &track, &disk);
}

void termRead(systemArgs * args)
{
   char * buff = args->arg1;
   int size = (long) args->arg2;
   int unit = (long) args->arg3;
   int returnValue = termReadReal(unit, size, buff);
}

void termWrite(systemArgs * args)
{
    char * buff = args->arg1;
    int size = (long) args->arg2;
    int unit = (long) args->arg3;
    int returnValue = termWriteReal(unit, size, buff);
}

//the following are real calls to the Syscalls
//I am pretty sure sleepReal returns -1 on error
int sleepReal(int seconds){
    if(debugflag4 && DEBUG4)
        USLOSS_Console("sleepReal(): started %i\n", getpid());

    updateProcTable(getpid());
    long wakeTime = USLOSS_Clock()+(seconds*1000000);
    ProcTable[getpid() % MAXPROC].WakeTime = wakeTime;
    addToSleepList(getpid(), &sleepList, wakeTime);

    // enableinterupts    
    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
   
    int result = MboxReceive(ProcTable[getpid() % MAXPROC].privateMBoxID, 0, 0);

    if( result != 0){
        USLOSS_Console("There was an error with MboxReceive\n");
    }

    return -1;
}

int diskReadReal(int unit, int track, int first, int sectors, void * buffer)
{
    return -1;
}

int diskWriteReal(int unit, int track, int first, int sectors, void * buffer)
{
    return -1;
}

int diskSizeReal(int unit, int *sector, int *track, int *disk)
{
    return -1;
}

int termReadReal(int unit, int size, char * buffer)
{
    return -1;
}

int termWriteReal(int unit, int size, char *text)
{
    return -1;
}

/* ------------------------------------------------------------------------
   Name - setUserMode
   Purpose - sets current mode to kernel Mode
   Parameters - nothing
   Returns - nothing
   Side Effects -none
   ----------------------------------------------------------------------- */
void setUserMode(){
    if(debugflag4&& DEBUG4)
        USLOSS_Console("setUserMode(): called\n");
    USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE );
}
/* ------------------------------------------------------------------------
   Name - check_kernel_mode
   Purpose - To check if a user level mode is invoked trying to execute kernel mode function
   Parameters - you can pass in the current process name to get a nice debug statement
   Returns - nothing
   Side Effects - Halts OS if system is not in kernel mode
   ----------------------------------------------------------------------- */
static void check_kernel_mode(char* name)
{
    if(debugflag4 && DEBUG4)
        USLOSS_Console("%s(): called check_kernel_mode\n", name);
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) {
        //not in kernel mode
        USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
        USLOSS_Console("check_kernel_mode\n");
        USLOSS_Halt(1);
    }

} /* check_kernel_mode */

/* ------------------------------------------------------------------------
   Name - initializeProcTable(maybe Incomplete?)
   Purpose - Initializes the process table
   Parameters -
   Returns -
   Side Effects - none.
   ----------------------------------------------------------------------- */
void initializeProcTable(){
    if(debugflag4 && DEBUG4)
        USLOSS_Console("initializeProcTable() called\n");
    int i;
    for(i =0; i<MAXPROC; i++){
        ProcTable[i] = (procStruct){
            .pid = -1,
            // .priority = -1,
            // .start_func = NULL,
            // .stackSize = -1,
            .nextSleepPtr = NULL,
            // .childSleepPtr = NULL,
            // .nextSiblingPtr = NULL,
            .privateMBoxID = MboxCreate(1,0),
            .WakeTime = -1,
        };
        // memset(ProcTable[i].name, 0, sizeof(char)*MAXNAME);
        // memset(ProcTable[i].startArg, 0, sizeof(char)*MAXARG);
    }
}

void updateProcTable(int pid){
    ProcTable[pid % MAXPROC].pid = pid;
}


void addToSleepList(int pid, procPtr *list, long time){
    //1st Case: if the list is empty
    if(*list == NULL)
    {
        *list = &ProcTable[pid%MAXPROC];
        return;
    }
    procPtr temp = *list;
    //2nd Case: list has only one item and this time is smaller than the first one.
    if(temp -> WakeTime > time)
    {
        ProcTable[pid%MAXPROC].nextSleepPtr = *list;
        *list = &ProcTable[pid%MAXPROC];
        return;
    }
    //3rd Case: put it somewhere on the list
    procPtr prev = NULL;
    procPtr curr = *list;
    while(curr->WakeTime < time)
    {
        prev = curr;
        curr = curr->nextSleepPtr;
        //if curr is null then we have reached the end of the list
        if(curr == NULL)
            break;
    }
    prev->nextSleepPtr = &ProcTable[pid%MAXPROC];
    ProcTable[pid%MAXPROC].nextSleepPtr = curr;
}
