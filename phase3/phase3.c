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
void wait_3(systemArgs*);
void terminate(systemArgs*);
int spawnReal(char *, int (*func)(char *), char *, unsigned int , int );
void spawnLaunch();
int waitReal(int *);
void terminateReal(int );
void initializeProcTable();
void initializeSystemCallVec();
void initializeSemTable();
void nullsys3();
void addtoChildList(procPtr , procPtr *);
void zapAndCleanAllChildren(procPtr *);
void cleanProcess(int);
static void setUserMode();
static void check_kernel_mode(char* );
void getTimeOfDay(systemArgs*);
int getTimeOfDayReal();
void getCPUTime(systemArgs*);
int getCPUTimeReal();
void getPID(systemArgs* sysArg);
int getPIDReal();
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
Purpose - Extracts arguments. 
Parameters - 
Returns - 
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
Parameters - 
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
    int childMboxID;

    if(debugflag3 && DEBUG3)
        USLOSS_Console("spawnReal(): Finished fork of %s, pid: %i\n", name, pid);
    //add to the procTable
    ProcTable[pid%MAXPROC].pid = pid;
    ProcTable[pid%MAXPROC].start_func = func;
    ProcTable[pid%MAXPROC].stackSize = stack_size;
    ProcTable[pid%MAXPROC].priority = priority;

    memcpy(ProcTable[pid%MAXPROC].name, name, strlen(name));
    addtoChildList(&ProcTable[pid&MAXPROC], &ProcTable[getpid()&MAXPROC].childProcPtr);
    if (arg != NULL){
        memcpy(ProcTable[pid%MAXPROC].startArg, arg, strlen(arg));
    }

    if (ProcTable[pid%MAXPROC].privateMBoxID == -1){
        // it does not exist
        childMboxID = MboxCreate(0,0);
    } else {
        // It does exist 
        childMboxID = ProcTable[pid%MAXPROC].privateMBoxID;
    }
    ProcTable[pid%MAXPROC].privateMBoxID = childMboxID;
    // Letting it's child go into the world (cry)
    if(debugflag3 && DEBUG3)
        USLOSS_Console("spawnReal(): %i might block on mbox %i, might wait for child.\n", pid, childMboxID);
    MboxReceive(childMboxID, NULL, 0);

    // I don't think this needs more stuff, the rest of the stuff gets done 
    // when spawnLaunch is finally called

    return pid;
}
    /* ------------------------------------------------------------------------
   Name - spawnLaunch(Incomplete)
   Purpose - This actually launches the spawn code. 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */
void spawnLaunch() {
    if(debugflag3 && DEBUG3)
        USLOSS_Console("spawnLaunch(): Started\n");
    
    int result;
    int pid = getpid();
    int procIndex = pid%MAXPROC;
    int childMboxID; 


    if (ProcTable[procIndex].privateMBoxID == -1){
        // it does not exist
        childMboxID = MboxCreate(0,0);
    } else {
        // It does exist 
        childMboxID = ProcTable[procIndex].privateMBoxID;
    }
    ProcTable[pid%MAXPROC].privateMBoxID = childMboxID;

    if(debugflag3 && DEBUG3)
        USLOSS_Console("SpawnLaunch(): %i might block on mbox %i, might wait for parent\n", pid, ProcTable[procIndex].privateMBoxID);
    // It will block here until the parent is ready to let it's child go, it's an emotional time.
    MboxSend(childMboxID, NULL, 0);


    // Switching to user mode.
    setUserMode();
    result = ProcTable[procIndex].start_func(ProcTable[procIndex].startArg);
    Terminate(result); // This may be wrong
}

   /* ------------------------------------------------------------------------
   Name - wait
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */
void wait_3(systemArgs *sysArg)
{
    int status;
    //need to check if process had children
    int pid = waitReal(&status);
    sysArg->arg1 = (void*) ( (long) pid);
    sysArg->arg2 = (void*) ( (long) status);
    sysArg->arg4 = (void*) ( (long) -1);

    setUserMode();
}

   /* ------------------------------------------------------------------------
   Name - terminate
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */
void terminate(systemArgs *sysArg){
    if(debugflag3 && DEBUG3)
        USLOSS_Console("terminate(): Started\n");
    terminateReal((int) sysArg->arg1);
}

    /* ------------------------------------------------------------------------
   Name - waitReal (Still do not know how to do this)
   Purpose - 
   Parameters - 
   Returns - 
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
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */
void terminateReal(int status){
    //More stuff to come
    if(debugflag3 && DEBUG3)
        USLOSS_Console("terminateReal(): called\n");
    zapAndCleanAllChildren(&ProcTable[getpid()%MAXPROC].childProcPtr);
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
    int value = sysArg->arg1;


    if (value < 0 ){
        sysArg->arg4 = -1;
        return;
    }

    int i = 0;
    while(SemTable[i].id != -1 && i < MAX_SEMS){

        i++;
    }

    if (i == MAX_SEMS){
        // The SemTable is full
        sysArg->arg4 = -1;  
        return;     
    }

    sysArg->arg4 = 0;
    int mBoxID = MboxCreate(value, 0);
    SemTable[i].mBoxID = mBoxID;
    SemTable[i].value = value;
    SemTable[i].id = i;

    sysArg->arg1 = i;
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
    int handler = sysArg->arg1;

    if(handler < 0 || handler > MAX_SEMS){
        sysArg->arg4 = -1;
        return;
    }


    sysArg->arg4 = 0;
    int mBoxID = SemTable[handler].mBoxID;
    MboxReceive(mBoxID, NULL, 0);

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

    int handler = sysArg->arg1;

    if(handler < 0 || handler > MAX_SEMS){
        sysArg->arg4 = -1;
        return;
    }


    sysArg->arg4 = 0;
    int mBoxID = SemTable[handler].mBoxID;
    MboxSend(mBoxID, NULL, 0);
    return;


}

    /* ------------------------------------------------------------------------
   Name - getTimeOfDay (Incomplete)
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */

void getTimeOfDay(systemArgs* sysArg){
    if(debugflag3 && DEBUG3)
        USLOSS_Console("getTimeOfDay(): called\n");
    int time = getTimeOfDayReal();
    sysArg->arg1 = (void*) ((long) time);
    return;
}
    /* ------------------------------------------------------------------------
   Name - getTimeOfDayReal (Incomplete)
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */

int getTimeOfDayReal(){
    return USLOSS_Clock();
}
    /* ------------------------------------------------------------------------
   Name - getCPUTime (Incomplete)
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */
void getCPUTime(systemArgs* sysArg) {
    if(debugflag3 && DEBUG3)
        USLOSS_Console("getCPUTime(): called\n");
    int time = getCPUTimeReal();
    sysArg->arg1 = (void*)((long) time);
    return;
}
/* ------------------------------------------------------------------------
   Name - getCPUTimeReal (Incomplete)
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */
int getCPUTimeReal() {
    return readtime();
}
    /* ------------------------------------------------------------------------
   Name - getPID (Incomplete)
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */
void getPID(systemArgs* sysArg) {
    if(debugflag3 && DEBUG3)
        USLOSS_Console("getCPUTime(): called\n");
    int pid = getPIDReal();
    sysArg->arg1 = (void*)((long) pid);
    return;
}
/* ------------------------------------------------------------------------
   Name - getPIDReal (Incomplete)
   Purpose - 
   Parameters - 
   Returns - 
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
            .privateMBoxID = -1,
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
    //These functions still need to be created

    //systemCallVec[SYS_SEMFREE]=
    systemCallVec[SYS_GETTIMEOFDAY]=(void *) getTimeOfDay;
    systemCallVec[SYS_CPUTIME]= (void *) getCPUTime;
    systemCallVec[SYS_GETPID]= (void *) getPID;
}

void initializeSemTable(){
    if(debugflag3 && DEBUG3)
        USLOSS_Console("Initializing ProcTable\n");
    int i;
    for(i =0; i<MAX_SEMS; i++){
        SemTable[i] = (semaphore){
            .id = -1,
            .value = -1,
            .blockedProcPtr = NULL,
            .mBoxID = -1,
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
        USLOSS_Console("addtoChildList(): called\n");
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

/* ------------------------------------------------------------------------
   Name - zapAndCleanAllChildren
   Purpose - To clear out the table on terminate
   Parameters - list of children
   Returns - nothing
   Side Effects - Halts OS if system is not in kernel mode
   ----------------------------------------------------------------------- */
void zapAndCleanAllChildren(procPtr *list)
{
    if(debugflag3 && DEBUG3)
        USLOSS_Console("zapAndCleanAllChildren(): called\n");
    if(*list == NULL)
        return;
    procPtr curr = *list;
    while(curr != NULL){
        int pid = curr->pid;
        zap(pid);
        curr = curr->nextSiblingPtr;
        cleanProcess(pid);
    }
    *list = NULL;
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
    memset(ProcTable[pid%MAXPROC].name, 0, sizeof(char)*MAXNAME);
    memset(ProcTable[pid%MAXPROC].startArg, 0, sizeof(char)*MAXARG);
}

/* ------------------------------------------------------------------------
   Name - setUserMode
   Purpose - sets current mode to kernel Mode
   Parameters -
   Returns - nothing
   Side Effects -
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

