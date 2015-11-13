#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <stdlib.h> /* needed for atoi() */
#include <stdio.h>
#include <phase3.h>
#include <phase4.h>
#include <provided_prototypes.h>
#include <string.h>
#include <driver.h>
int 	running;

static int	ClockDriver(char *);
static int	DiskDriver(char *);
static int  TermDriver(char *);
static int  TermReader(char *);
static int  TermWriter(char *);
/* the process table */
procStruct ProcTable[MAXPROC];
procPtr sleepList;
int termPrivMailbox[USLOSS_TERM_UNITS][USLOSS_TERM_UNITS];
int termProcIDTable[USLOSS_TERM_UNITS][3];

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
        sprintf(name, "DiskDriver_%d", i);
        pid = fork1(name, DiskDriver, buf, USLOSS_MIN_STACK, 2);
        // Maybe storing the pid's into an array so you can clean them up later
        if (pid < 0) {
            USLOSS_Console("start3(): Can't create disk driver %d\n", i);
            USLOSS_Halt(1);
        }
        // Adding disk Proc drivers to Proc table
        ProcTable[pid%MAXPROC].pid = pid;
    }

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
    int j;
    for(i=0; i<USLOSS_TERM_UNITS; i++)
    {
        for(j=0; j<USLOSS_TERM_UNITS; j++)
        {
            termPrivMailbox[i][j] = MboxCreate(10, (sizeof(char) * MAXLINE));
        }
    }
    for(i = 0; i < USLOSS_TERM_UNITS; i++) {
        sprintf(buf, "%d", i);
        pid = fork1(name, TermReader, buf, USLOSS_MIN_STACK, 2);
        termProcIDTable[i][1] = pid;
        sempReal(running);
     //   pid = fork1(name, TermWriter, buf, USLOSS_MIN_STACK, 2);
     //   termProcIDTable[i][2] = pid;
     //   sempReal(running);
        pid = fork1(name, TermDriver, buf, USLOSS_MIN_STACK, 2);
        termProcIDTable[i][0] = pid;
        sempReal(running);
    }
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

    status = 0;
    zap(clockPID);  // clock driver
    ProcTable[clockPID % MAXPROC].pid = -1;
    //zapTermReaders
    for(i = 0; i < USLOSS_TERM_UNITS; i++)
    {
        MboxCondSend(termPrivMailbox[i][0], "0", 0);
        zap(termProcIDTable[i][1]);
    }
    //zapTermWriters
    //for(i=0; i<USLOSS_TERM_UNITS; i++)
    //{
    //    zap(termProcIDTable[i][2]);
    //}
    //enable read Interrupts
    for(i =0; i < USLOSS_TERM_UNITS; i++)
    {
        int ctrl = 0;
        ctrl = USLOSS_TERM_CTRL_RECV_INT(ctrl);
        USLOSS_DeviceOutput(USLOSS_TERM_DEV, i, (void *)((long) ctrl));
    }
    //fopen stuff here; not sure what to open tho
    //zapTermDriver
    char filename[100];
    for(i=0; i<USLOSS_TERM_UNITS; i++)
    {
        sprintf(filename, "term%d.in", i);
        FILE *f = fopen(filename, "a+");
        fprintf(f, "last line\n");
        fflush(f);
        //fclose(f);
    //    USLOSS_Console("%d\n", termProcIDTable[i][0]);
        fclose(f);
        zap(termProcIDTable[i][0]);
       // fclose(f);
    }
    // Zap everything else
    for (int i = 0; i < MAXPROC; ++i){
        if (ProcTable[i % MAXPROC].pid != -1){
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
        while(proc != NULL && proc->WakeTime<= currentTime){
            // Send to free a process
            MboxSend(ProcTable[proc->pid % MAXPROC].privateMBoxID, 0, 0);
            proc = proc->nextSleepPtr;
            sleepList = proc;
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

static int TermDriver(char * arg)
{
    int unit = atoi((char *) arg);
    if(debugflag4 && DEBUG4)
        USLOSS_Console("TermDriver(%d): started\n", unit);
    int result;
    int status;
    semvReal(running);
    enableInterrupts();
    while(!isZapped())
    {
        result = waitDevice(USLOSS_TERM_DEV, unit, &status);
        if(result != 0)
        {
            return 0;
        }
        result = USLOSS_TERM_STAT_RECV(status);
        if(result == USLOSS_DEV_BUSY) {
            MboxSend(termPrivMailbox[unit][0], &status,sizeof(int));
        }
        //NEED TO DO TERMWRITER
    }
    return 0;
}

static int TermReader(char * arg)
{
    int unit = atoi((char*) arg);
    if(debugflag4 && DEBUG4)
        USLOSS_Console("TermReader(%d): started\n", unit);
    char line[MAXLINE+1];
    memset(line, '\0', MAXLINE+1);
    int count = 0;
    semvReal(running);
    while(!isZapped())
    {
        int ch;
        int num = MboxReceive(termPrivMailbox[unit][0], &ch,sizeof(int));
        char message = USLOSS_TERM_STAT_CHAR(ch);
        line[count]= message;
        count++;
        if(message=='\n' || count == MAXLINE)
        {
            line[count] = '\0';
            MboxCondSend(termPrivMailbox[unit][1], &line, count);
            memset(line, '\0', MAXLINE+1);
            count = 0;
        }
    }
    return 0;
}

static int TermWriter(char * arg)
{
    int unit = atoi((char *) arg);
    if(debugflag4 && DEBUG4)
        USLOSS_Console("TermWriter(%d): started\n", unit);
    int result, status;
    semvReal(running);
    while(!isZapped())
    {

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
   char * buff = (char *) args->arg1;
   int size = (long) args->arg2;
   int unit = (long) args->arg3;
   int returnValue = termReadReal(unit, size, buff);
   args->arg2 = (void *) ((long) returnValue);
   args->arg4 = (void *) ((long) 0);
   setUserMode();
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
    if (seconds <= 0){
        return -1;
    }
    updateProcTable(getpid());
    int wakeTime = USLOSS_Clock()+(seconds*1000000);
    ProcTable[getpid() % MAXPROC].WakeTime = wakeTime;
    addToSleepList(getpid(), &sleepList, wakeTime);
    int result = MboxReceive(ProcTable[getpid() % MAXPROC].privateMBoxID, 0, 0);
    if( result != 0){
        USLOSS_Console("There was an error with MboxReceive\n");
    }
    // We don't care about it after this point
    ProcTable[getpid() % MAXPROC].pid = -1;
    return 0;
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
    int i;
    for(i =0; i < USLOSS_TERM_UNITS; i++)
    {
        int ctrl = 0;
        ctrl = USLOSS_TERM_CTRL_RECV_INT(ctrl);
        USLOSS_DeviceOutput(USLOSS_TERM_DEV, i, (void *)((long) ctrl));
    }
    char buf[MAXLINE+1];
    for(i = 0; i<MAXLINE+1; i++)
    {
        buf[i] = '\0';
    }
    USLOSS_Console("%s", buffer);
    int num = MboxReceive(termPrivMailbox[unit][1], &buf, MAXLINE+1);
    memcpy(buffer, buf,(sizeof(char)*num)+1 ); //do strncpy from mboxreceive
    return num;
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
            .nextSleepPtr = NULL,
            .privateMBoxID = MboxCreate(1,0),
            .mboxTermDriver = -1,
            .mboxTermReal = -1,
            .WakeTime = -1,
        };
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

static void enableInterrupts()
{
    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
}
