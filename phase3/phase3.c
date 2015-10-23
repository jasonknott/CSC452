#include <stdlib.h>
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <usyscall.h>
#include <string.h>
#include <libuser.h>
#include <sems.h>
//---------------------------- Function Prototypes ----------------------------
extern int start3(char *);
void spawn(systemArgs*);
void spawnLaunch();
int  spawnReal(char *, int (*func)(char *), char *, unsigned int , int );
void wait_3(systemArgs*);
int  waitReal(int *);
void terminate(systemArgs*);
void terminateReal(int );
void semCreate(systemArgs*);
int semCreateReal(int);
void semP(systemArgs*);
void semPReal(int);
void semV(systemArgs*);
void semVReal(int);
void semFree(systemArgs*);
int semFreeReal(int);
void nullsys3();
void addtoChildList(procPtr , procPtr *);
void addtoProcList(procPtr node, procPtr *list);
void popProcList(procPtr*);
void zapAndCleanAllChildren(procPtr );
void zapAndCleanAllProc(procPtr *);
void removeFromChildrenList(int, procPtr *, int);
void removeFromParentList(int );
void cleanProcess(int);
static void setUserMode();
static void check_kernel_mode(char* );
void getTimeOfDay(systemArgs*);
int getTimeOfDayReal();
void getCPUTime(systemArgs*);
int getCPUTimeReal();
void getPID(systemArgs* sysArg);
int getPIDReal();
void initializeProcTable();
void initializeSystemCallVec();
void initializeSemTable();
//-----------------------------------------------------------------------------

/* the process table */
procStruct ProcTable[MAXPROC];

semaphore SemTable[MAX_SEMS];
int debugflag3 = 0;

int start2(char *arg)
{
    int pid;
    int status;
    /*
     * Check kernel mode here.
     */

     check_kernel_mode("start3");

    /*
     * Data structure initialization as needed...
     */
     //Initialize ProcTable
     initializeProcTable();
     // Initialize SystemCallVec
     initializeSystemCallVec();
     // Initialize SemTable
     initializeSemTable();

    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * Assumes kernel-mode versions of the system calls
     * with lower-case names.  I.e., Spawn is the user-mode function
     * called by the test cases; spawn is the kernel-mode function that
     * is called by the syscallHandler; spawnReal is the function that
     * contains the implementation and is called by spawn.
     *
     * Spawn() is in libuser.c.  It invokes USLOSS_Syscall()
     * The system call handler calls a function named spawn() -- note lower
     * case -- that extracts the arguments from the sysargs pointer, and
     * checks them for possible errors.  This function then calls spawnReal().
     *
     * Here, we only call spawnReal(), since we are already in kernel mode.
     *
     * spawnReal() will create the process by using a call to fork1 to
     * create a process executing the code in spawnLaunch().  spawnReal()
     * and spawnLaunch() then coordinate the completion of the phase 3
     * process table entries needed for the new process.  spawnReal() will
     * return to the original caller of Spawn, while spawnLaunch() will
     * begin executing the function passed to Spawn. spawnLaunch() will
     * need to switch to user-mode before allowing user code to execute.
     * spawnReal() will return to spawn(), which will put the return
     * values back into the sysargs pointer, switch to user-mode, and
     * return to the user code that called Spawn.
     */
    pid = spawnReal("start3", start3, NULL, USLOSS_MIN_STACK, 3);

    if(debugflag3 && DEBUG3)
        USLOSS_Console("start2(): spawned start3 PID #%i\n", pid);

    /* Call the waitReal version of your wait code here.
     * You call waitReal (rather than Wait) because start2 is running
     * in kernel (not user) mode.
     */
    pid = waitReal(&status);
    return -1;
} /* start2 */

/* ------------------------------------------------------------------------
Name - spawn
Purpose - Extracts arguments for the Spawn Syscall
Parameters -systemArgs(fork1 arguments and address of pid)
Returns - pid in arg1 and 0(if success or -1 if fails) in arg4
Side Effects - none.
----------------------------------------------------------------------- */
void spawn(systemArgs *sysArg){
    if(debugflag3 && DEBUG3)
        USLOSS_Console("spawn(): Started\n");
    int (*func)(char *) = sysArg->arg1;
    char *arg = sysArg->arg2;
    int stack_size = (long) sysArg->arg3;
    int priority = (long) sysArg->arg4;
    char *name = sysArg->arg5;
    //Need to do a validity check on sysArgs for arg4
    // More error checking like this
    if (stack_size < USLOSS_MIN_STACK){
        sysArg->arg4 = (void *) ( (long) -1);
        return;
    }
    // Switch to kernal mode
    int pid = spawnReal(name, func, arg, stack_size, priority);
    setUserMode();
    sysArg->arg1 = (void*) ( (long) pid);
    sysArg->arg4 = (void *) ( (long) 0);
    return;
}
/* ------------------------------------------------------------------------
Name - spawnReal(Incomplete)
Purpose - Calls fork1 then spawnLaunch to launch code
Parameters - arguments of fork1 
    char *name, int (*func)(char *), char *arg, unsigned int stack_size, int priority
Returns - the pid given from fork1
Side Effects - none.

spawnReal() will create the process by using a call to fork1 to
create a process executing the code in spawnLaunch().  

spawnReal() and spawnLaunch() then coordinate the completion
of the phase 3 process table entries needed for the new process.

spawnReal() will return to the original caller of Spawn, while
spawnLaunch() will begin executing the function passed to Spawn. 

spawnLaunch() will need to switch to user-mode before allowing 
user code to execute. 

spawnReal() will return to spawn(),
which will put the return values back into the sysargs pointer,
switch to user-mode, and  return to the user code that called Spawn.

----------------------------------------------------------------------- */
int spawnReal(char *name, int (*func)(char *), char *arg, unsigned int stack_size, int priority)
{
    if(debugflag3 && DEBUG3)
        USLOSS_Console("spawnReal(): Started\n");

    if(debugflag3 && DEBUG3)
        USLOSS_Console("spawnReal(): About to fork %s at priority %i\n", name, priority);

    int pid = fork1(name, (void *)spawnLaunch, arg, stack_size, priority);

    if(debugflag3 && DEBUG3)
        USLOSS_Console("spawnReal(): Finished fork of %s, pid: %i\n", name, pid);
    //add to the procTable
    ProcTable[pid%MAXPROC].pid = pid;
    ProcTable[pid%MAXPROC].start_func = func;
    ProcTable[pid%MAXPROC].stackSize = stack_size;
    ProcTable[pid%MAXPROC].priority = priority;
    ProcTable[pid%MAXPROC].parentPid = getpid();

    memcpy(ProcTable[pid%MAXPROC].name, name, strlen(name));
    addtoChildList(&ProcTable[pid%MAXPROC], &ProcTable[getpid()%MAXPROC].childProcPtr);
    if (arg != NULL){
        memcpy(ProcTable[pid%MAXPROC].startArg, arg, strlen(arg));
    }

    if (ProcTable[pid%MAXPROC].priority < ProcTable[getpid()%MAXPROC].priority){
        // it does not exist
        if(debugflag3 && DEBUG3)
            USLOSS_Console("spawnReal(): %s block on mbox %i, waiting for child.\n", ProcTable[getpid()%MAXPROC].name, ProcTable[pid%MAXPROC].privateMBoxID);
        
        // Letting it's child go into the world (cry)
        MboxSend(ProcTable[pid%MAXPROC].privateMBoxID, NULL, 0);
        
        if(debugflag3 && DEBUG3)
            USLOSS_Console("spawnReal(): %s got passed from mbox %i.\n", ProcTable[getpid()%MAXPROC].name, ProcTable[pid%MAXPROC].privateMBoxID);
    }


    // I don't think this needs more stuff, the rest of the stuff gets done
    // when spawnLaunch is finally called

    return pid;
}
    /* ------------------------------------------------------------------------
   Name - spawnLaunch(Incomplete)
   Purpose - This actually launches the spawn code and terminates.
   Parameters - none
   Returns - none
   Side Effects - none.
   ----------------------------------------------------------------------- */
void spawnLaunch() {
    if(debugflag3 && DEBUG3)
        USLOSS_Console("spawnLaunch(): Started %i\n", getpid());

    int result;
    int pid = getpid();

    if (ProcTable[pid%MAXPROC].pid == -1){
        // it does not exist
        if(debugflag3 && DEBUG3)
            USLOSS_Console("spawnLaunch(): %i block on mbox %i, wait for parent\n", pid, ProcTable[pid%MAXPROC].privateMBoxID);
        
        // It will block here until the parent is ready to let it's child go, it's an emotional time.
            MboxReceive(ProcTable[pid%MAXPROC].privateMBoxID, NULL, 0);
        
        if(debugflag3 && DEBUG3)
            USLOSS_Console("spawnLaunch(): %s got passed from mbox %i.\n", ProcTable[pid%MAXPROC].name, ProcTable[pid%MAXPROC].privateMBoxID);

    }

    // Switching to user mode.
    if(!isZapped()){
        setUserMode();
        result = ProcTable[pid%MAXPROC].start_func(ProcTable[pid%MAXPROC].startArg);
        Terminate(result); // This may be wrong
    }else {
        terminateReal(0);
    }
}

   /* ------------------------------------------------------------------------
   Name - wait
   Purpose - This parses the arguments for wait_3.
   Parameters - sysArg (passes in the address of status) arg2
   Returns -returns the pid(arg1), status(arg2), and (-1 if success and bleh for failure) in arg4
   Side Effects - none.
   ----------------------------------------------------------------------- */
void wait_3(systemArgs *sysArg){
    if(debugflag3 && DEBUG3)
        USLOSS_Console("wait_3(): Started\n");
    int status;
    //need to check if process had children
    int pid = waitReal(&status);
    if(!isZapped()) {
        sysArg->arg1 = (void*) ( (long) pid);
        sysArg->arg2 = (void*) ( (long) status);
        sysArg->arg4 = (void*) ( (long) -1);
        setUserMode();
        return;
    }else {
        terminateReal(0);
    }
}

   /* ------------------------------------------------------------------------
   Name - terminate
   Purpose - parses the system arguments for terminateReal, which is the status
             terminate.
   Parameters - systemArgs. arg1 holds the status
   Returns - nothing
   Side Effects - none.
   ----------------------------------------------------------------------- */
void terminate(systemArgs *sysArg){
    if(debugflag3 && DEBUG3)
        USLOSS_Console("terminate(): Started\n");
    terminateReal((long) sysArg->arg1);
}

    /* ------------------------------------------------------------------------
   Name - waitReal (Still do not know how to do this)
   Purpose - waits until child process quits.
   Parameters - status. passes in the address of staus in join
   Returns - returns the status of the join
   Side Effects - none.
   ----------------------------------------------------------------------- */
int waitReal(int * status) {
    if(debugflag3 && DEBUG3)
        USLOSS_Console("waitReal(): Started\n");
    int r_stat = join(status);
    return r_stat;
}
    /* ------------------------------------------------------------------------
   Name - terminateReal (Incomplete)
   Purpose - to terminate the current process that is running.
   Parameters - status of the quit.
   Returns - HELLFIRE.
   Side Effects - none.
   ----------------------------------------------------------------------- */
void terminateReal(int status){
    //More stuff to come
    if(debugflag3 && DEBUG3)
        USLOSS_Console("terminateReal(): called\n");
    zapAndCleanAllChildren(ProcTable[getpid()%MAXPROC].childProcPtr);
    removeFromParentList(ProcTable[getpid()%MAXPROC].parentPid);
    cleanProcess(getpid());
    quit(status);
}


/* ------------------------------------------------------------------------
   Name - semCreate (Incomplete)
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */
void semCreate(systemArgs *sysArg){
    if(debugflag3 && DEBUG3)
        USLOSS_Console("semCreate(): Started\n");
    int value = (long) sysArg->arg1;

    if (value < 0 ){
        sysArg->arg4 = (void*)(long) -1;
        return;
    }

    int id = semCreateReal(value);
    
    if (id == MAX_SEMS){
        // The SemTable is full
        sysArg->arg4 = (void*)(long) -1;       
    } else{
        sysArg->arg4 = (void*)(long) 0;
        sysArg->arg1 = (void*)(long) id;
    }

    if(debugflag3 && DEBUG3)
        USLOSS_Console("semCreate(): Created semaphore %i with value %i using mbox %i\n", sysArg->arg1, value, SemTable[id].priv_mBoxID);

    return;

}

/* ------------------------------------------------------------------------
   Name - semCreateReal (Incomplete)
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */
int semCreateReal(int value){
    
    
    // Picking a slot in the table
    int i = 0;
    while(SemTable[i].id != -1 && i < MAX_SEMS){
        i++;
    }

    if (i == MAX_SEMS){
        return i;
    }


    int priv_mBoxID = MboxCreate(value, 0);
    int mutex_mBoxID = MboxCreate(1, 0);

    MboxSend(mutex_mBoxID, NULL, 0);
    
    SemTable[i].priv_mBoxID =   priv_mBoxID;
    SemTable[i].mutex_mBoxID =  mutex_mBoxID;
    SemTable[i].value =         value;
    SemTable[i].startingValue = value;
    SemTable[i].id =            i;

    // Send lots of stuff to the mail box so people can pick it up and go on
    int j;
    for (j = 0; j < value; ++j){
        MboxSend(priv_mBoxID, NULL, 0);
    }
    // USLOSS_Console("SemTable[i].id %i\n", i);

    MboxReceive(SemTable[i].mutex_mBoxID, NULL, 0);

    return SemTable[i].id;
}

/* ------------------------------------------------------------------------
   Name - semP (Incomplete)
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.

   Subtracting
   ----------------------------------------------------------------------- */
void semP(systemArgs *sysArg){
    if(debugflag3 && DEBUG3)
        USLOSS_Console("semP(): Started\n");

    int handler = (long)sysArg->arg1;

    // Error checking
    if(handler < 0 || handler > MAX_SEMS){
        sysArg->arg4 = (void*)(long)-1;
        return;
    }else{
        sysArg->arg4 = 0;
    }
    if(debugflag3 && DEBUG3)
        USLOSS_Console("semP(): handler = %i\n", handler);

    semPReal(handler);


    return;
}


void semPReal(int handler){
    if(debugflag3 && DEBUG3)
        USLOSS_Console("semPReal(): Started\n");

    MboxSend(SemTable[handler].mutex_mBoxID, NULL, 0);

    if (SemTable[handler].value == 0){
        // Add to blocked list
        addtoProcList(&ProcTable[getpid()%MAXPROC], &SemTable[handler].blockedProcPtr);
        // block here on private mail
        if(debugflag3 && DEBUG3)
            USLOSS_Console("semP(): About to block on mailBox %i\n", SemTable[handler].priv_mBoxID);


        MboxReceive(SemTable[handler].mutex_mBoxID, NULL, 0);

        int result = MboxReceive(SemTable[handler].priv_mBoxID, NULL, 0);


        MboxSend(SemTable[handler].mutex_mBoxID, NULL, 0);


        if(result < 0){
            USLOSS_Console("semp(): SOMETHING WENT VERY WRONG\n");
        } 
        if(debugflag3 && DEBUG3)
            USLOSS_Console("semP(): %s just freed from mailBox %i\n",ProcTable[getpid()%MAXPROC].name, SemTable[handler].priv_mBoxID);

        if (SemTable[handler].id == -1){
            // This means the sem was freed
            if(debugflag3 && DEBUG3)
                USLOSS_Console("semP(): mbox was freed, process Terminating\n");
            MboxReceive(SemTable[handler].mutex_mBoxID, NULL, 0);
            terminateReal(1); //I'm not sure why it's a 1, but that's what the test wanted
        }

    } else{
        SemTable[handler].value -= 1;
        if(debugflag3 && DEBUG3)
                USLOSS_Console("semP(): about to MboxReceive on mailbox %i, should't block\n", SemTable[handler].priv_mBoxID);
        int result = MboxReceive(SemTable[handler].priv_mBoxID, NULL, 0);
        if(result < 0){
            USLOSS_Console("semp(): SOMETHING WENT VERY WRONG\n");
        } 
        if(debugflag3 && DEBUG3)
            USLOSS_Console("semP(): Subtracting... value == %i\n", SemTable[handler].value);
    }
    

    MboxReceive(SemTable[handler].mutex_mBoxID, NULL, 0);
    // ERROR CHECKING WITH ZAPS NEED TO GO HERE
    

    return;
}

/* ------------------------------------------------------------------------
   Name - semV (Incomplete)
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.

   Adding
   ----------------------------------------------------------------------- */
void semV(systemArgs *sysArg){
    if(debugflag3 && DEBUG3)
        USLOSS_Console("semV(): Started\n");

    int handler = (long)sysArg->arg1;

    // Error checking
    if(handler < 0 || handler > MAX_SEMS){
        sysArg->arg4 = (void*)(long)-1;
        return;
    }else{
        sysArg->arg4 = 0;
    }

    semVReal(handler);
    return;
}

void semVReal(int handler){
    if(debugflag3 && DEBUG3)
        USLOSS_Console("semVReal(): Started\n");

    MboxSend(SemTable[handler].mutex_mBoxID, NULL, 0);

    /*If you try to send too many times to a mailbox it will block,
    we don't want that so I'm checking if it's more than that and 
    just adding and leaving if it is.*/
    if (SemTable[handler].value > SemTable[handler].startingValue){
        SemTable[handler].value += 1;
        return;
    }

    /*It doesn't matter if the value is 0, but  it does matter if
    people are blocked on the sem. If people are blocked, free them
    and don't change the count.*/
    if (SemTable[handler].blockedProcPtr != NULL){
        // procPtr proc = SemTable[handler].blockedProcPtr;
        popProcList(&SemTable[handler].blockedProcPtr);
        if(debugflag3 && DEBUG3)
            USLOSS_Console("semV(): Sending to mbox %i\n", SemTable[handler].priv_mBoxID);
        
        MboxReceive(SemTable[handler].mutex_mBoxID, NULL, 0);


        MboxSend(SemTable[handler].priv_mBoxID, NULL, 0);

        MboxSend(SemTable[handler].mutex_mBoxID, NULL, 0);
        if(debugflag3 && DEBUG3)
            USLOSS_Console("semV(): Running after sending to mBoxID %i\n", SemTable[handler].priv_mBoxID);
    } else {
        // Else add value
        SemTable[handler].value += 1;
        if(debugflag3 && DEBUG3)
            USLOSS_Console("semV(): Adding ... value == %i\n", SemTable[handler].value);
    }
    MboxReceive(SemTable[handler].mutex_mBoxID, NULL, 0);

    return;
}


/* ------------------------------------------------------------------------
   Name - semFree (Incomplete)
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */
void semFree(systemArgs *sysArg){
    if(debugflag3 && DEBUG3)
        USLOSS_Console("semFree(): Started\n");

    
    int handler = (long)sysArg->arg1;

    if(handler < 0 || handler > MAX_SEMS){
        sysArg->arg4 = (void*)(long) -1;
        return;
    }


    int rtnValue = semFreeReal(handler);

    sysArg->arg4 = (void*)(long) rtnValue;

    return;

}

int semFreeReal(int handler){
    if(debugflag3 && DEBUG3)
        USLOSS_Console("semFreeReal(): Started\thandler = %i\n",handler);

    int mutex = SemTable[handler].mutex_mBoxID;
    // MboxSend(mutex, NULL, 0);


    int priv_mBoxID = SemTable[handler].priv_mBoxID;
    procPtr blocked = SemTable[handler].blockedProcPtr;


    // clearing out SemTable
    SemTable[handler].id = -1;
    SemTable[handler].value = -1;
    SemTable[handler].startingValue = -1;
    SemTable[handler].blockedProcPtr = NULL;
    SemTable[handler].priv_mBoxID = -1;
    SemTable[handler].mutex_mBoxID = -1;
    SemTable[handler].free_mBoxID = -1;

    if(blocked != NULL){
        while(blocked != NULL){
            // There are blocked processes
            // Let them all go by sending to mailbox
            // USLOSS_Console("semFreeReal(): about to free %s on mbox %i\n", blocked->name, priv_mBoxID);
            // USLOSS_Console("semFreeReal(): Current priority %i\n", ProcTable[getpid()].priority);
            // USLOSS_Console("semFreeReal(): priority of %s %i\n", blocked->name, blocked->priority);
            procPtr next = blocked->nextProcPtr;
            popProcList(&blocked);

            // MboxReceive(mutex, NULL, 0);
            int result = MboxSend(priv_mBoxID, NULL, 0);
            // MboxSend(mutex, NULL, 0);
            blocked = next;
            if (result < 0){
                USLOSS_Console("semFreeReal(): THERE HAS BEEN AN ERROR\n");
            }
        }
        // MboxReceive(mutex, NULL, 0);
        return 1;
    }
    else{
        // MboxReceive(mutex, NULL, 0);
        return 0;
    }



}

    /* ------------------------------------------------------------------------
   Name - getTimeOfDay (Incomplete)
   Purpose - get the USLOSS version of getTimeOfDay
   Parameters - systemArgs (nothing real)
   Returns - in sysArg->arg1 is set to the time.
   Side Effects - none.
   ----------------------------------------------------------------------- */

void getTimeOfDay(systemArgs* sysArg){
    if(debugflag3 && DEBUG3)
        USLOSS_Console("getTimeOfDay(): called\n");
    int time = getTimeOfDayReal();
    if(!isZapped()) {
        sysArg->arg1 = (void*) ((long) time);
        setUserMode();
        return;
    }else
        terminateReal(0);
}
    /* ------------------------------------------------------------------------
   Name - getTimeOfDayReal (Incomplete)
   Purpose - get True usless clock time
   Parameters - nada
   Returns - USLOSS_Clock
   Side Effects - none.
   ----------------------------------------------------------------------- */

int getTimeOfDayReal(){
    return USLOSS_Clock();
}
    /* ------------------------------------------------------------------------
   Name - getCPUTime (Incomplete)
   Purpose - getCPUTIME from the depths of hell where no second has ever been.
   Parameters - systemArgs(passer for addresses)
   Returns - CPUTime in sysArg->arg1
   Side Effects - none.
   ----------------------------------------------------------------------- */
void getCPUTime(systemArgs* sysArg) {
    if(debugflag3 && DEBUG3)
        USLOSS_Console("getCPUTime(): called\n");
    int time = getCPUTimeReal();
    if(!isZapped()) {
        sysArg->arg1 = (void*)((long) time);
        setUserMode();
        return;
    }else {
        terminateReal(0);
    }
}
/* ------------------------------------------------------------------------
   Name - getCPUTimeReal (Incomplete)
   Purpose - get CPUTime
   Parameters - niet
   Returns - readtime
   Side Effects - none.
   ----------------------------------------------------------------------- */
int getCPUTimeReal() {
    return readtime();
}
    /* ------------------------------------------------------------------------
   Name - getPID (Incomplete)
   Purpose - following suite from above. separates the getPIDReal from getPID;
   Parameters - systemArgs
   Returns - pid of the current process in sysArg->arg1
   Side Effects - none.
   ----------------------------------------------------------------------- */
void getPID(systemArgs* sysArg) {
    if(debugflag3 && DEBUG3)
        USLOSS_Console("getPID(): called\n");
    int pid = getPIDReal();
    if(!isZapped()) {
        sysArg->arg1 = (void*)((long) pid);
        setUserMode();
        return;
    } else {
        terminateReal(0);
    }
}
/* ------------------------------------------------------------------------
   Name - getPIDReal (Incomplete)
   Purpose - gets the PID
   Parameters - nothing.
   Returns - pid of the current process
   Side Effects - none.
   ----------------------------------------------------------------------- */
int getPIDReal() {
    return getpid();
}

    /* ------------------------------------------------------------------------
   Name - initializeProcTable(maybe Incomplete?)
   Purpose - Initializes the process table
   Parameters -
   Returns -
   Side Effects - none.
   ----------------------------------------------------------------------- */
void initializeProcTable(){
    if(debugflag3 && DEBUG3)
        USLOSS_Console("Initializing ProcTable\n");
    int i;
    for(i =0; i<MAXPROC; i++){
        ProcTable[i] = (procStruct){
            .pid = -1,
            .priority = -1,
            .start_func = NULL,
            .stackSize = -1,
            .nextProcPtr = NULL,
            .childProcPtr = NULL,
            .nextSiblingPtr = NULL,
            .privateMBoxID = MboxCreate(0,0),
        };
        memset(ProcTable[i].name, 0, sizeof(char)*MAXNAME);
        memset(ProcTable[i].startArg, 0, sizeof(char)*MAXARG);
    }
}
    /* ------------------------------------------------------------------------
   Name - initializeSystemCallVec(Incomplete)
   Purpose - Initializes all the systemCallVec
   Parameters -
   Returns -
   Side Effects - none.
   ----------------------------------------------------------------------- */
void initializeSystemCallVec(){
    if(debugflag3 && DEBUG3)
        USLOSS_Console("Initializing SystemCallVec\n");
    int i;
    for(i=0; i<= USLOSS_MAX_SYSCALLS; i++)
        systemCallVec[i] = nullsys3;

    systemCallVec[SYS_SPAWN] = (void *)spawn;
    systemCallVec[SYS_WAIT] = (void *)wait_3;
    systemCallVec[SYS_TERMINATE] = (void *) terminate;
    systemCallVec[SYS_SEMCREATE]=(void *)semCreate;
    systemCallVec[SYS_SEMP]=(void *)semP;
    systemCallVec[SYS_SEMV]=(void *)semV;
    systemCallVec[SYS_SEMFREE]=(void *)semFree;
    systemCallVec[SYS_GETTIMEOFDAY]=(void *) getTimeOfDay;
    systemCallVec[SYS_CPUTIME]= (void *) getCPUTime;
    systemCallVec[SYS_GETPID]= (void *) getPID;
}


/* ------------------------------------------------------------------------
   Name - initializeSemTable 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */
void initializeSemTable(){
    if(debugflag3 && DEBUG3)
        USLOSS_Console("Initializing ProcTable\n");
    int i;
    for(i =0; i<MAX_SEMS; i++){
        SemTable[i] = (semaphore){
            .id = -1,
            .value = -1,
            .startingValue = -1,
            .blockedProcPtr = NULL,
            .priv_mBoxID = -1,
            .mutex_mBoxID = -1,
            .free_mBoxID = -1
        };

    }
}

    /* ------------------------------------------------------------------------
   Name - nullsys3 (Incomplete)
   Purpose - Unused system Calls for this phase should just point to here. Need to do more later.
   Parameters -
   Returns -
   Side Effects - Halts system.
   ----------------------------------------------------------------------- */
void nullsys3(){
    USLOSS_Console("nullsys3(): called. Halting...\n");
    USLOSS_Halt(1);
}
/*-------------------------------------------------------------------------
 *  Name - addtoChildList
 *  Purpose - adds to queue that is first come first serve base
 *  Parameters - the node that we are adding and the list that it is being added to.
 *  Returns - nothing
 *  Side Effects - should be fine
 *  --------------------------------------------------------------------------- */
void addtoChildList(procPtr node, procPtr *list) {
    if(debugflag3 && DEBUG3)
        USLOSS_Console("addtoChildList(): called for the parent pid: %i\n", getpid());
    if(*list == NULL) {
        *list = node;
        return;
    }
    procPtr curr = *list;
    while(curr->nextSiblingPtr != NULL) {
        curr = curr -> nextSiblingPtr;
    }
    curr->nextSiblingPtr = node;
    node->nextSiblingPtr = NULL;
    return;
}


/*-------------------------------------------------------------------------
 *  Name - addtoProcList
 *  Purpose - adds to queue that is first come first serve base
 *  Parameters - the node that we are adding and the list that it is being added to.
 *  Returns - nothing
 *  Side Effects - should be fine
 *  --------------------------------------------------------------------------- */
void addtoProcList(procPtr node, procPtr *list) {
    if(debugflag3 && DEBUG3)
        USLOSS_Console("addtoProcList(): called\n");
 
    if(*list == NULL) {
        *list = node;
        return;
    }
    procPtr curr = *list;
    while(curr->nextProcPtr != NULL) {
        curr = curr -> nextProcPtr;
    }
    curr->nextProcPtr = node;
    node->nextProcPtr = NULL;
    return;
}

/* ------------------------------------------------------------------------
   Name - initializeSemTable 
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */
void popProcList(procPtr *list) {
  if(*list == NULL)
      return;
  procPtr temp = *list;
  *list = temp->nextProcPtr;
}

/* ------------------------------------------------------------------------
   Name - zapAndCleanAllChildren
   Purpose - To clear out the table on terminate
   Parameters - list of children
   Returns - nothing
   Side Effects - Halts OS if system is not in kernel mode
   ----------------------------------------------------------------------- */
void zapAndCleanAllChildren(procPtr list)
{
    if(debugflag3 && DEBUG3)
        USLOSS_Console("zapAndCleanAllChildren(): called\n");
    if(list == NULL)
        return;
    procPtr curr = list;
    while(curr != NULL){
        procPtr temp = curr->nextSiblingPtr;
        int pid = curr->pid;
        if(pid >= 0)
            zap(pid);
        curr = temp;
        //cleanProcess(pid);
    }
    //*list = NULL;
    return;
}



/* ------------------------------------------------------------------------
   Name - zapAndCleanAllChildren
   Purpose - To clear out the table on terminate
   Parameters - list of children
   Returns - nothing
   Side Effects - Halts OS if system is not in kernel mode
   ----------------------------------------------------------------------- */
void zapAndCleanAllProc(procPtr *list)
{
    if(debugflag3 && DEBUG3)
        USLOSS_Console("zapAndCleanAllProc(): called\n");
    if(*list == NULL)
        return;
    procPtr curr = *list;
    while(curr != NULL){
        int pid = curr->pid;
        zap(pid);
        curr = curr->nextProcPtr;
    }
    return;
}
/* ------------------------------------------------------------------------
   Name - removeFromParentList
   Purpose - if the child calls Terminate, it will remove itself from the
             parent's list
   Parameters - parent_pid
   Returns - nothing
   Side Effects - nothing
   ----------------------------------------------------------------------- */
void removeFromParentList(int parent_pid)
{
    if(debugflag3 && DEBUG3)
        USLOSS_Console("\tremoveFromParentList(): called for parent pid: %i\n", parent_pid);
    if(getpid() == 4 || parent_pid == -1)
        //if it is start3 you do not want to remove it from the parent's list
        return;
    else
    {
        removeFromChildrenList(getpid(),&ProcTable[parent_pid%MAXPROC].childProcPtr, parent_pid);
    }
}
/* ------------------------------------------------------------------------
   Name - removeFromChildrenList
   Purpose - removes a specific pid from the children's list
   Parameters - list of children, and pid that needs to be removed
   Returns - nothing
   Side Effects - Halts OS if system is not in kernel mode
   ----------------------------------------------------------------------- */
void removeFromChildrenList(int pid, procPtr * list, int parent_pid)
{
    if(debugflag3 && DEBUG3)
        USLOSS_Console("\tremoveFromChildrenList(): called for pid: %i\n", pid);
    if(*list == NULL) {
        if(debugflag3 && DEBUG3) {
            USLOSS_Console("Error! Children List is NULL!!!!!\n");
            USLOSS_Console("Parent Pid: %i\n", parent_pid);
        }
        return;
     //   USLOSS_Halt(1);
    }
    if(((procPtr) *list)->pid == pid)
    {
        procPtr temp = *list;
        *list = temp->nextSiblingPtr;
        temp->nextSiblingPtr = NULL;
        temp->parentPid = -1;
        return;
    }
    procPtr prev = *list;
    procPtr curr = *list;
    while(curr->pid != pid)
    {
        prev = curr;
        curr = curr->nextSiblingPtr;
    }
    prev->nextSiblingPtr = curr->nextSiblingPtr;
    curr->nextSiblingPtr= NULL;
    curr->parentPid = -1;
    return;
}
/* ------------------------------------------------------------------------
   Name - cleanProcess
   Purpose - To clear out the process from the table
   Parameters - pid
   Returns - nothing
   Side Effects - Halts OS if system is not in kernel mode
   ----------------------------------------------------------------------- */
void cleanProcess(int pid)
{
    if(debugflag3 && DEBUG3)
        USLOSS_Console("cleanProcess(): called\n");
    ProcTable[pid%MAXPROC].pid = -1;
    ProcTable[pid%MAXPROC].priority = -1;
    ProcTable[pid%MAXPROC].start_func = NULL;
    ProcTable[pid%MAXPROC].stackSize = -1;
    ProcTable[pid%MAXPROC].nextProcPtr = NULL;
    ProcTable[pid%MAXPROC].childProcPtr = NULL;
    ProcTable[pid%MAXPROC].nextSiblingPtr = NULL;
    // ProcTable[pid%MAXPROC].privateMBoxID = -1;
    ProcTable[pid%MAXPROC].parentPid = -1;
    memset(ProcTable[pid%MAXPROC].name, 0, sizeof(char)*MAXNAME);
    memset(ProcTable[pid%MAXPROC].startArg, 0, sizeof(char)*MAXARG);
}

/* ------------------------------------------------------------------------
   Name - setUserMode
   Purpose - sets current mode to kernel Mode
   Parameters - nothing
   Returns - nothing
   Side Effects -none
   ----------------------------------------------------------------------- */
void setUserMode(){
    if(debugflag3&& DEBUG3)
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
    if(debugflag3 && DEBUG3)
        USLOSS_Console("%s(): called check_kernel_mode\n", name);
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) {
        //not in kernel mode
        USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
        USLOSS_Console("check_kernel_mode\n");
        USLOSS_Halt(1);
    }

} /* check_kernel_mode */

