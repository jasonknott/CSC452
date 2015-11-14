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
/* the process table */
procStruct ProcTable[MAXPROC];
procPtr sleepList;
int diskTrackSize[2];
procPtr diskQ[2][2];
int currArmDir[2];
int currArmTrack[2];
int diskPid[USLOSS_DISK_UNITS];
#define UP 0
#define DOWN 1
int     diskRequestMutex[2];
#define ABS(a,b) a-b > 0 ? a-b : -(a-b)


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
    char    name[128];
    char    termbuf[10];
    char    buf[5];
    int     i;
    int     clockPID;
    int     pid;
    int     status;

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
    diskQ[1][UP] = NULL;
    diskQ[1][DOWN] = NULL;
    diskQ[0][UP] = NULL;
    diskQ[0][DOWN] = NULL;
    currArmDir[0] = NULL;
    currArmDir[1] = NULL;
    diskRequestMutex[0] = MboxCreate(1, 0);
    diskRequestMutex[1] = MboxCreate(1, 0);
    // diskQMutex = MboxCreate(1, 0);

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
        USLOSS_Console("start3(): about to fork diskDrivers\n");

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
        diskPid[i] = pid;
    }

    if(debugflag4 && DEBUG4)
        USLOSS_Console("start3(): Finished forking diskDrivers\n");

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

    zap(clockPID);  // clock driver
    ProcTable[clockPID % MAXPROC].pid = -1;
    if(debugflag4 && DEBUG4)
        USLOSS_Console("start3(): Freeing diskDrivers\n");    

    // Free the disk drivers...
    for (int i = 0; i < USLOSS_DISK_UNITS; ++i){
        MboxSend(diskRequestMutex[i], 0, 0);
        MboxSend(diskRequestMutex[i], 0, 0);
    }
    

    status = 0;
    // zap(clockPID);  // clock driver
    // ProcTable[clockPID % MAXPROC].pid = -1;
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
            if(debugflag4 && DEBUG4)
                USLOSS_Console("start3(): About to zap %i\n", i); 
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
        USLOSS_Console("DiskDriver(): started %i\n", getpid());
    // Initize a bunch of stuf
    int unit = atoi( (char *) arg);     // Unit is passed as arg.
    int result;
    int status;
    USLOSS_DeviceRequest r = (USLOSS_DeviceRequest){
        .opr = USLOSS_DISK_TRACKS,
        .reg1 = &diskTrackSize[unit],
    };
    USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &r);
    waitDevice(USLOSS_DISK_DEV, unit, &status);

    // let partent know everything is ready! Might need to enable interrupts
    semvReal(running); 

    while(!isZapped()) {
        // Block until something wakes it up
        // MboxReceive(ProcTable[getpid() % MAXPROC].privateMBoxID, 0, 0);
        MboxReceive(diskRequestMutex[unit], 0, 0); 
        if(debugflag4 && DEBUG4)
            USLOSS_Console("DiskDriver(): has been woken up\n");


        if(debugflag4 && DEBUG4){
            USLOSS_Console("DiskDriver(): Got past mutex\n");
            USLOSS_Console("DiskDriver(): pid of first proc on UP list %i track %i on unit %i\n", 
                diskQ[unit][UP] != NULL ? diskQ[unit][UP]->pid : -1,
                diskQ[unit][UP] != NULL ? diskQ[unit][UP]->track : -1, unit);
            USLOSS_Console("DiskDriver(): pid of first proc on DOWN list %i track %i on unit %i\n",
                diskQ[unit][DOWN] != NULL ? diskQ[unit][DOWN]->pid : -1,
                diskQ[unit][DOWN] != NULL ? diskQ[unit][DOWN]->track : -1, unit);
        }


        // Do stuff here
        if (diskQ[unit][UP] == NULL &&  diskQ[unit][DOWN] == NULL){
            if(debugflag4 && DEBUG4)
                USLOSS_Console("DiskDriver(): There has been an error, empty diskList. Or shutting down driver\n");
            break; //I'm not sure about this...
        }
    }
    return 0;
}

<<<<<<< HEAD
        if(diskQ[unit][currArmDir[unit]] == NULL){
            /*This means there is nothing waiting that direction
            So I'm changing the direction of the arm
            If it was up it's now down, was down, now up
            */
            currArmDir[unit] = currArmDir[unit] + 1 % 2;
        }
        procPtr proc = diskQ[unit][currArmDir[unit]];

        status = diskDoWork(proc, unit);
        // I'm sure I'll need the status somehow
        // diskQ[unit][currArmDir[unit]]->status = status;
        diskQ[unit][currArmDir[unit]] = diskQ[unit][currArmDir[unit]]->nextProcPtr;     
        proc->nextProcPtr = NULL;
=======
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
>>>>>>> 63c030a7e14d352498b844b1f0c0e9f422ac089a

        // any more clean up after??

        // MboxSend(diskRequestMutex, 0, 0);

        if(debugflag4 && DEBUG4)
            USLOSS_Console("DiskDriver(): Wake up process %i\n", proc->pid);
        MboxSend(ProcTable[proc->pid % MAXPROC].privateMBoxID, &status, sizeof(int));

    }

<<<<<<< HEAD
    return 0;
=======
void termRead(systemArgs * args)
{
   char * buff = (char *) args->arg1;
   int size = (long) args->arg2;
   int unit = (long) args->arg3;
   int returnValue = termReadReal(unit, size, buff);
   args->arg2 = (void *) ((long) returnValue);
   args->arg4 = (void *) ((long) 0);
   setUserMode();
>>>>>>> 63c030a7e14d352498b844b1f0c0e9f422ac089a
}


//the following are real calls to the Syscalls
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

int diskReadReal(int unit, int track, int first, int sectors, void * buffer){
    if(debugflag4 && DEBUG4)
        USLOSS_Console("diskReadReal(): started pid %i unit %i\n", getpid(), unit);
    // ERROR CHECING

    if (unit < 0 || 1 < unit){
        return -1;
    }
    if (track < 0 || diskTrackSize[unit] <= track){
        return -1;
    }
    if (sectors < 0 || USLOSS_DISK_TRACK_SIZE <= sectors){
        return -1;
    }


    ProcTable[getpid() % MAXPROC].opr = USLOSS_DISK_READ;

    return diskRequest(unit, track, first, sectors, buffer);
    // return 0;
}

int diskWriteReal(int unit, int track, int first, int sectors, void * buffer){
    if(debugflag4 && DEBUG4)
        USLOSS_Console("diskWriteReal(): started pid %i unit %i\n", getpid(), unit);

    // USLOSS_Console("Unit %i track %i, sectors %i\n", unit, track, sectors);
    // USLOSS_Console("Unit %i track %i, sectors %i\n", unit, diskTrackSize[unit], USLOSS_DISK_TRACK_SIZE);
    // ERROR CHECING
    if (unit < 0 || 1 < unit){
        return -1;
    }
    if (track < 0 || diskTrackSize[unit] <= track){
        return -1;
    }
    if (sectors < 0 || USLOSS_DISK_TRACK_SIZE <= first){
        return -1;
    }

    ProcTable[getpid() % MAXPROC].opr = USLOSS_DISK_WRITE;
   
    return diskRequest(unit, track, first, sectors, buffer);
    // return 0;
}


int diskRequest(int unit, int track, int first, int sectors, void * buffer){
    int status;
    // A helper function because read and write are absically the same 
    updateProcTable(getpid());

    ProcTable[getpid() % MAXPROC].track = track;
    ProcTable[getpid() % MAXPROC].first = first;
    ProcTable[getpid() % MAXPROC].sectors = sectors;
    ProcTable[getpid() % MAXPROC].buffer = buffer;


    if(debugflag4 && DEBUG4){
        USLOSS_Console("diskRequest(): track %i currArmTrack %i\n", track, currArmTrack[unit]);
        USLOSS_Console("diskRequest(): Arm is moving %s \n", currArmDir[unit] == UP ? "UP" : "DOWN");
    }   
    if (track >= currArmTrack[unit]){
        if(debugflag4 && DEBUG4){
            USLOSS_Console("diskRequest(): adding to %s list \n", currArmDir[unit]==UP ? "UP" : "DOWN");
        }
        // Add to list needs to be updated depending on the currArmTrack
        addToList(getpid(), &diskQ[unit][currArmDir[unit]], unit);
    // ◦ Insert process based on track order onto the diskQ[unit][diskPosition[unit]].
    } else{
        if(debugflag4 && DEBUG4){
            USLOSS_Console("diskRequest(): adding to %s list \n", (currArmDir[unit] + 1) % 2== UP ? "up" : "down");
        }
        addToList(getpid(), &diskQ[unit][currArmDir[unit] + 1 % 2], unit);
    // ◦ Insert process based on track order onto the diskQ[unit][(diskPosition[unit] + 1) % 2].
    }
    if(debugflag4 && DEBUG4){
        USLOSS_Console("diskRequest(): print UP\n");
        printProcList(&diskQ[unit][UP]);
        USLOSS_Console("diskRequest(): print DOWN\n");
        printProcList(&diskQ[unit][DOWN]);
    }

    if(debugflag4 && DEBUG4)
        USLOSS_Console("diskRequest(): about to free disk driver %i\n", (diskPid[unit]) % MAXPROC);
    MboxSend(diskRequestMutex[unit], 0, 0);

    // Block yourself until driver is finished
    MboxReceive(ProcTable[getpid() % MAXPROC].privateMBoxID, &status, sizeof(int));
    if(debugflag4 && DEBUG4)
        USLOSS_Console("diskRequest(): Back to user process %i\n", getpid()); 

    // We don't care about it any more
    ProcTable[getpid() % MAXPROC].pid = -1;
    return status;
}

int diskDoWork(procPtr request, int unit){
    if(debugflag4 && DEBUG4)
        USLOSS_Console("diskDoWork(): started %i\n", getpid()); 
    USLOSS_DeviceRequest req;
    int numSectors, firstTrack, currentSector, currentTrack, operation;

    void *diskBuf, *diskBufTemp;
    int status;
    //grab everything from the request...
    //I might need to look these up in proctablr
    numSectors = request->sectors; 
    firstTrack = request->track;
    currentTrack = firstTrack;
    currentSector = request->first;
    diskBuf = request->buffer;
    diskBufTemp = request->buffer;

    while(numSectors > 0){
        if(debugflag4 && DEBUG4){
            USLOSS_Console("diskDoWork(): %s sector %i\n", 
                request->opr == USLOSS_DISK_READ ? "reading" : "writing" ,currentSector); 
        }

        if (firstTrack != currArmTrack[unit]){
            // move the arm to the right track
            req.opr = USLOSS_DISK_SEEK;
            req.reg1 = firstTrack;
            currArmTrack[unit] = firstTrack;
            USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &req);
            waitDevice(USLOSS_DISK_DEV, unit, &status);
        }
        if(debugflag4 && DEBUG4)
            USLOSS_Console("diskDoWork(): Arm is at track %i for unit %i\n", currArmTrack[unit], unit);

        req.opr = request->opr;
        req.reg1 = currentSector;
        req.reg2 = diskBuf;
        USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &req);
        // USLOSS_Console("about to waitDevice\n");
        waitDevice(USLOSS_DISK_DEV, unit, &status); //Return the status of this
        // USLOSS_Console("finished waitDevice\n");
        currentSector++;

        if(currentSector >= diskTrackSize[unit]){
            // This handles wrap around if you finish a track it goes to the next track
            // and if you finish all the tracks you start at beginning again. 
            currentSector = 0;
            currentTrack = (currentTrack + 1) % diskTrackSize[unit];
            // diskTrackSize[unit] is set in device driver
        }
        diskBuf += USLOSS_DISK_SECTOR_SIZE;
        numSectors --; //go through all the sectors you wre asked
    }

    if(debugflag4 && DEBUG4){
        // // THis output only will work for strings
        // USLOSS_Console("diskDoWork(): disk %i has %s %s\n", 
        //     unit, request->opr == USLOSS_DISK_READ ? "read" : "written" , diskBuf);
    }
    return status;
}

int diskSizeReal(int unit, int *sector, int *track, int *disk){
    if(debugflag4 && DEBUG4)
        USLOSS_Console("diskSizeReal(): started %i\n", getpid());

    *sector = USLOSS_DISK_SECTOR_SIZE;
    *track = USLOSS_DISK_TRACK_SIZE;
    *disk = diskTrackSize[unit];

    return 0;
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
static void check_kernel_mode(char* name){
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
            .nextProcPtr = NULL,
            // .nextSiblingPtr = NULL,
            .privateMBoxID = MboxCreate(1,sizeof(int)),
            .privateMBoxID = MboxCreate(1,0),
            .mboxTermDriver = -1,
            .mboxTermReal = -1,

            .WakeTime = -1,
            .opr = 1,
            .track = -1,
            .first = -1,
            .sectors = -1,
            .buffer = -1,
        };
    }
}

void updateProcTable(int pid){
    // lol
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

void addToList(int pid, procPtr *list, int unit){
    //1st Case: if the list is empty
    if(*list == NULL){
        *list = &ProcTable[pid%MAXPROC];
        return;
    }

    //2nd Case: list has only one item and this time is smaller than the first one.
    procPtr temp = *list;
    if(temp->track > ProcTable[pid%MAXPROC].track){
        ProcTable[pid%MAXPROC].nextProcPtr = *list;
        *list = &ProcTable[pid%MAXPROC];
        return;
    }

    //3rd Case: add to end of list
    procPtr prev = NULL;
    procPtr curr = *list;
    int distance = ABS(ProcTable[pid%MAXPROC].track, currArmTrack[unit]);
    int tempDis = diskTrackSize[unit];
    // USLOSS_Console("adding track: %i currTrack %i\n", ProcTable[pid%MAXPROC].track, currArmTrack[unit]);
    while(curr != NULL && distance < tempDis){
        tempDis = ABS(curr->track, currArmTrack[unit]);
        // USLOSS_Console("tempDis %i\n", tempDis);
        prev = curr;
        curr = curr->nextProcPtr;
    }
    prev->nextProcPtr = &ProcTable[pid%MAXPROC];
    ProcTable[pid%MAXPROC].nextProcPtr = curr;
}

printProcList(procPtr *list){
    USLOSS_Console("PRINT THIS LIST!\n");
    procPtr curr = *list;
    while(curr != NULL){
        USLOSS_Console("\tpid %i-track %i",curr->pid, curr->track);
        curr = curr->nextProcPtr;
    }
    USLOSS_Console("\n===============END=============\n");

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


    if(returnValue > 0){
        args->arg1 = (void*)((long)returnValue);
        args->arg4 = (void*)((long)0);
    } else {
        args->arg1 = (void*)((long)0);
        args->arg4 = (void*)((long)returnValue);
    }
}

void diskWrite(systemArgs * args)
{
    void * buff = args->arg1;
    int sectors = (long) args->arg2;
    int track = (long) args->arg3;
    int first = (long) args->arg4;
    int unit = (long) args->arg5;
    int returnValue = diskWriteReal(unit, track, first, sectors, buff);
    if(returnValue > 0){
        args->arg1 = (void*)((long)returnValue);
        args->arg4 = (void*)((long)0);
    } else {
        args->arg1 = (void*)((long)0);
        args->arg4 = (void*)((long)returnValue);
    }
}

void diskSize(systemArgs * args)
{
    int unit = (long) args->arg1;
    int sector = 0;
    int track = 0;
    int disk = 0;
    diskSizeReal(unit, &sector, &track, &disk);

    args->arg1 = disk;
    args->arg2 = sector;
    args->arg3 = track;
    // args->arg1 = disk;

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
static void enableInterrupts()
{
    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
}

