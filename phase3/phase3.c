#include <stdlib.h>
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <usyscall.h>
#include <string.h>
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
void nullsys3();
static void check_kernel_mode(char* );
//-----------------------------------------------------------------------------

/* the process table */
procStruct ProcTable[MAXPROC];
int debugflag3 = 1;

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
     //Initialize SystemCallVec
     initializeSystemCallVec();
     //Do mboxCreate Stuff

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
    int i = (long) sysArg->arg4;
    USLOSS_Console("spawn(): pri: %i\n", i);
    int (*func)(char *) = sysArg->arg1;
    char *arg = sysArg->arg2;
    int stack_size = sysArg->arg3;
    int priority = ((int*)sysArg->arg4);
    char *name = sysArg->arg5;
    //Need to do a validity check on sysArgs for arg4
    // More error checking like this
    if (stack_size < USLOSS_MIN_STACK){
        sysArg->arg4 = -1;
        return;
    }

    // Switch to kernal mode
    USLOSS_PsrSet( USLOSS_PsrGet() | USLOSS_PSR_CURRENT_MODE );


    int pid = spawnReal(name, func, arg, stack_size, priority);
    sysArg->arg1 = (void*) ( (long) pid);
    sysArg->arg4 = (void *) ( (long) 0);
    USLOSS_Console("spawn(): about to return\n");
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
    //int parentpid = getpid(); //need to add to the childrens list
    int pid = fork1(name, (void *)spawnLaunch, arg, stack_size, priority);
    //add to the procTable
    ProcTable[pid%MAXPROC] = (procStruct){
        .pid = pid,
        .start_func = func,
        .stackSize = stack_size,
        .priority = priority
    };
    memcpy(ProcTable[pid%MAXPROC].name, name, strlen(name));
    if (arg != NULL){
        memcpy(ProcTable[pid%MAXPROC].startArg, arg, strlen(arg));
    }


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
    int procIndex = getpid()%MAXPROC;
    //There should be some kind of Semaphore call here
    

    // Switching to user mode.
    USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE );

    result = ProcTable[procIndex].start_func(ProcTable[procIndex].startArg);
    
    terminateReal(result); // This may be wrong
}

   /* ------------------------------------------------------------------------
   Name - wait
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */
void wait_3(systemArgs *sysArg)
{   int status;
    //need to check if process had children
    int pid = waitReal(&status);
    sysArg->arg1 = (void*) ( (long) pid);
    sysArg->arg2 = (void*) ( (long) status);
    sysArg->arg4 = (void*) ( (long) -1);
}

   /* ------------------------------------------------------------------------
   Name - terminate
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */
void terminate(systemArgs *sysArg)
{
    terminateReal((int) sysArg.arg1);
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


    join(status);
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
    quit(status);
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
            .nextSiblingPtr = NULL
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
        //These functions still need to be created
    //systemCallVec[SYS_SEMCREATE]=
    //systemCallVec[SYS_SEMP]=
    //systemCallVec[SYS_SEMV]=
    //systemCallVec[SYS_SEMFREE]=
    //systemCallVec[SYS_GETTIMEOFDAY]=
    //systemCallVec[SYS_CPUTIME]=
    //systemCallVec[SYS_GETPID]=
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

