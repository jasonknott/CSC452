/*
 * phase5.c
  
  Programmers: David Christy


 */
#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <usyscall.h>
#include <libuser.h>
#include <stdlib.h>
#include <vm.h>
#include <string.h>
#include "provided_prototypes.h"



int debugflag5 = 1;


//externs
extern void mbox_create(systemArgs *args_ptr);
// Found this in libuser.c
extern int Mbox_Create(int numslots, int slotsize, int *mboxID);
extern void mbox_release(systemArgs *args_ptr);
extern void mbox_send(systemArgs *args_ptr);
extern void mbox_receive(systemArgs *args_ptr);
extern void mbox_condsend(systemArgs *args_ptr);
extern void mbox_condreceive(systemArgs *args_ptr);

// Functions
static void FaultHandler(int type, void* offset);
static void vmInit(systemArgs *sysargsPtr);
void * vmInitReal(int, int, int, int);
static void vmDestroy(systemArgs *sysargsPtr);
void vmDestroyReal(void);
static int Pager(char *buf);
void PrintStats();
void setUserMode();
void initializeProcTable();
void addToList(int, procPtr*);
void printProcList(procPtr*);

// Globals
int sector;
int track;
int disk;
int faultMailbox;
VmStats  vmStats;
static Process procTable[MAXPROC];
FTE* frameTable;
procPtr listOfPagers;
int numOfFrames;
int numOfPages;
int numOfPagers;
int diskBlocks;
int frameArm;

FaultMsg faults[MAXPROC]; /* Note that a process can have only
                           * one fault at a time, so we can
                           * allocate the messages statically
                           * and index them by pid. */
void* vmRegion;

/*
 *----------------------------------------------------------------------
 *
 * start4 --
 *
 * Initializes the VM system call handlers.
 *
 * Results:
 *      MMU return status
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
int start4(char *arg){
    int pid;
    int result;
    int status;

    /* to get user-process access to mailbox functions */
    systemCallVec[SYS_MBOXCREATE]      = mbox_create;
    systemCallVec[SYS_MBOXRELEASE]     = mbox_release;
    systemCallVec[SYS_MBOXSEND]        = mbox_send;
    systemCallVec[SYS_MBOXRECEIVE]     = mbox_receive;
    systemCallVec[SYS_MBOXCONDSEND]    = mbox_condsend;
    systemCallVec[SYS_MBOXCONDRECEIVE] = mbox_condreceive;

    /* user-process access to VM functions */
    systemCallVec[SYS_VMINIT]          = vmInit;
    systemCallVec[SYS_VMDESTROY]       = vmDestroy;


    // Find stuff out about the disk
    DiskSize(1, &sector, &track, &disk);

    diskBlocks = disk;

    initializeProcTable();

    result = Spawn("Start5", start5, NULL, 8*USLOSS_MIN_STACK, 2, &pid);
    if (result != 0) {
        USLOSS_Console("start4(): Error spawning start5\n");
        Terminate(1);
    }
    result = Wait(&pid, &status);
    if (result != 0) {
        USLOSS_Console("start4(): Error waiting for start5\n");
        Terminate(1);
    }
    Terminate(0);
    return 0; // not reached

} /* start4 */




/*
 *----------------------------------------------------------------------
 *
 * vmInitReal --
 *
 * Called by vmInit.
 * Initializes the VM system by configuring the MMU and setting
 * up the page tables.
 *
 * Results:
 *      Address of the VM region.
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
void * vmInitReal(int mappings, int pages, int frames, int pagers){
    if (debugflag5 && DEBUG5)
      USLOSS_Console("vmInit(): started\n");
    //more error checking
    
    int status;
    int dummy;
    
    // Set the globals
    numOfFrames = frames;
    numOfPages =  pages;
    numOfPagers = pagers;

    //check mode
    CheckMode();
    
    // Initialize USLOSS backend of VM 
    status = USLOSS_MmuInit(mappings, pages, frames);
    if (status != USLOSS_MMU_OK) {
      USLOSS_Console("vmInitReal: couldn't init MMU, status %d\n", status);
      abort();
    }
    
    // Interupt stuff
    USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;
    
    // Get the address the vmRegion
    vmRegion = USLOSS_MmuRegion(&dummy);

    // Initalize frame table
    frameTable = malloc(frames * sizeof(FTE));
    for (int i = 0; i < frames; i++) {
        frameTable[i].state = UNUSED;
        frameTable[i].pid = -1;
        frameTable[i].page = -1;
        // track and frame
        frameTable[i].track = i/2;
        frameTable[i].sector = i%2 * 8;
        frameTable[i].referenced = FALSE;
        frameTable[i].clean = TRUE;
    }

    /*
    * Initialize page tables.
    */
    if (debugflag5 && DEBUG5)
      USLOSS_Console("vmInit(): Initialize page tables.\n");
    for (int i = 0; i < MAXPROC; ++i){
        procTable[i].pages = 0;
        procTable[i].pageTable = malloc(sizeof(PTE) * pages);
        for (int j = 0; j < pages; ++j){
          procTable[i].pageTable[j].state = UNUSED;
          procTable[i].pageTable[j].frame = -1;
          procTable[i].pageTable[j].diskBlock = -1;
        }
    }

    /* 
    * Create the fault mailbox.
    */
    if (debugflag5 && DEBUG5)
      USLOSS_Console("vmInit(): Create the fault mailbox.\n");
    faultMailbox = MboxCreate(0, sizeof(FaultMsg));
    if (faultMailbox < 0){
      USLOSS_Console("There has been an error with Mbox_Create\n");
    }

    /*
    * Fork the pagers.
    */
    char str[16];
    if (debugflag5 && DEBUG5)
      USLOSS_Console("vmInit(): About to fork pagers\n");
    for (int i = 0; i < pagers; ++i){
      // USLOSS_MIN_STACK might change
      // int   fork1(char *name, int(*func)(char *), char *arg, int stacksize, int priority);
      if (debugflag5 && DEBUG5)
        USLOSS_Console("About to creat Pager%i\n", i);
      sprintf(str, "%d", i);
      int pid = fork1("Pager", Pager, str, USLOSS_MIN_STACK, 2);

      procTable[pid % MAXPROC].pid = pid;
      addToList(pid, &listOfPagers);
    }
    // Output the list of pages
    if (debugflag5 && DEBUG5)
      printProcList(&listOfPagers);

    /*
    * Zero out, then initialize, the vmStats structure
    */
    if (debugflag5 && DEBUG5)
      USLOSS_Console("vmInit(): initialize the vmStats structure\n");
    memset((char *) &vmStats, 0, sizeof(VmStats));
    
    
    /*
    * Initialize other vmStats fields.
    */
    vmStats.pages = pages;
    vmStats.frames = frames;
    vmStats.diskBlocks = diskBlocks;
    vmStats.freeFrames = frames;
    vmStats.freeDiskBlocks = diskBlocks;
    vmStats.switches = 0;
    vmStats.faults = 0;
    vmStats.new = 0;
    vmStats.pageIns = 0;
    vmStats.pageOuts = 0;
    vmStats.replaced = 0;

    //Filling vmRegion with 0's
    // DON'T MOVE!!! This has black magic in it, and it scares me....
    memset(vmRegion, 0, USLOSS_MmuPageSize() * pages);


    vmStarted = TRUE;

    return vmRegion;
} /* vmInitReal */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroyReal --
 *
 * Called by vmDestroy.
 * Frees all of the global data structures
 *
 * Results:
 *      None
 *
 * Side effects:
 *      The MMU is turned off.
 *
 *----------------------------------------------------------------------
 */
void vmDestroyReal(void){
  if (debugflag5 && DEBUG5)
    USLOSS_Console("vmDestroyReal(): started\n");
  
  CheckMode();
  USLOSS_MmuDone();
  /*
  * Kill the pagers here.
  */
  procPtr curr = listOfPagers;
  FaultMsg dummy;
  while(curr != NULL){
    if (debugflag5 && DEBUG5)
      USLOSS_Console("vmDestroyReal(): zapping %i\n", curr->pid);
    // Send message to free proc
    MboxSend(faultMailbox, &dummy, 0);
    // And then zap it!
    zap(curr->pid);
    curr = curr->nextProcPtr;
  }


  /*
  * Print vm statistics.
  */
  PrintStats(); 
} /* vmDestroyReal */

/*
 *----------------------------------------------------------------------
 *
 * FaultHandler
 *
 * Handles an MMU interrupt. Simply stores information about the
 * fault in a queue, wakes a waiting pager, and blocks until
 * the fault has been handled.
 *
 * Results:
 * None.
 *
 * Side effects:
 * The current process is blocked until the fault is handled.
 *
 *----------------------------------------------------------------------
 */
static void FaultHandler(int type /* USLOSS_MMU_INT */, void* arg  /* Offset within VM region */){
  if (debugflag5 && DEBUG5)
    USLOSS_Console("FaultHandler(): started %i\n", getpid());

  int cause;
  // This was given to us, don't look too closely unless or it will look back
  assert(type == USLOSS_MMU_INT);
  cause = USLOSS_MmuGetCause();
  assert(cause == USLOSS_MMU_FAULT);
  vmStats.faults++;

  FaultMsg fault = (FaultMsg){
    .pid = getpid(),
    .addr = arg, // Address that caused the fault.
    .replyMbox = procTable[getpid() % MAXPROC].FaultMBoxID,  // Mailbox to send reply.
    // Add more stuff here.
  };

  faults[getpid() % MAXPROC] = fault;

  
  // About to free a Pager, they will pick up the rest of the task
  if (debugflag5 && DEBUG5)
    USLOSS_Console("FaultHandler(): About to send fault %i to faultMailbox %i\n", fault.pid, faultMailbox);
  int result = MboxSend(faultMailbox, &fault, sizeof(FaultMsg));
  if (result < 0){
    USLOSS_Console("There has been an error sending to faultMailbox,\n");
  }

  // Block this guy until the pager finishes
  if (debugflag5 && DEBUG5)
    USLOSS_Console("FaultHandler(): About to Block on FaultMBoxID %i\n", procTable[getpid() % MAXPROC].FaultMBoxID);
  int status;
  MboxReceive(procTable[getpid() % MAXPROC].FaultMBoxID, &status, sizeof(int));



} /* FaultHandler */

/*
 *----------------------------------------------------------------------
 *
 * Pager
 *
 * Kernel process that handles page faults and does page replacement.
 *
 * Results:
 * None.
 *
 * Side effects:
 * None.
 *
 *----------------------------------------------------------------------
 */
static int Pager(char *buf){
  if (debugflag5 && DEBUG5){
      USLOSS_Console("Pager%s(): started\n", buf);
  }
  FaultMsg fault;  
    // int pagerID = ati(buf);
  // void *bufDisk = malloc(USLOSS_MmuPageSize());

  while(TRUE) {
    if (debugflag5 && DEBUG5)
      USLOSS_Console("Pager%s(): Top of loop\n", buf);

    /* Wait for fault to occur (receive from mailbox) */
    // Mbox_Receive(int mboxID, void *msgPtr, int msgSize);
    MboxReceive(faultMailbox, &fault, sizeof(FaultMsg));
    if (debugflag5 && DEBUG5)
      USLOSS_Console("Pager%s(): Just got message from faultMailbox\n", buf);

    // check if zapped while waiting
    if (isZapped()) {
      break;
    }

    int page = (int) ((long) fault.addr / USLOSS_MmuPageSize()); // convert to a page
    int pid = fault.pid;

    // FindFrame
    // int frame = findFrame(pagerID (for debug));

    // if frameTable[frame].status == UNUSED:
    //   vmStats.freeFrames--;
    // else:
        // The frame is used...
        // get access with USELOSS call
        // int referencebit of frame
        // USLOSS_MmuGetAccess(frame, &referencebit of frame)
        // if (referencebit (bit wise and)  DIRTY MACRO)
              // diskBlock = outputFrame()
              // store diskBlock in old pagetable Entry 
        // oldProcess.pageTable.frameNum = -1;
        // if proctable[pid].pagetable->diskBlock == -1:
              // it's not on the disk
              // USLOSS_MmuMap(0, 0, frame, USLOSS_MMU_PROT_RW);
              // memset(vmRegion, \0, USLOSS_MmuPageSize());
        // else
              // getFrame(pid, page, difDisk);
              // USLOSS_MmuMap(0, 0 framenum, RW)
              // memcyp(vmRegion, bufDisk, USLOSS_MmuPageSize)

    // USLOSS_mmu_setaccess(frame, 0);
    // USLOSS_Mmu_UnMap(tag (0), 0, frame, USLOSS_MMU_PROT_RW)
    
    // cleaning up the frametable
    // frameTable[frame].pid = pid
    // frameTable[frame].dirty = 0
    // frameTable[frame].ref = 1
    // frameTable[frame].page = page


    // /* Unblock waiting (faulting) process */
    // result = MboxSend(fault.replyMbox, "", 0);
    // if (result < 0) {
    //   USLOSS_Console("There bas been an error\n");
    // }

  }
  quit(0);
  return 0;
} /* Pager */


void getFrame(int pid,int page,void *bufDisk){
  // get diskblock from disk
  void diskblock = procTable[pid].pagetable[page]->diskblock;
  // always use disk1
  // diskblock == is the track the page is stored on
  // diskreadReal(1, diskblock, 0, 8, bufDisk)


}


int findFrame(int pagerID){

  /* Look for free frame */
  int frame;
  int freeFrames = vmStats.freeFrames != 0;
  for (frame = 0; frame < numOfFrames && freeFrames; ++frame){
    if(frameTable[frame].status == UNUSED){
      freeFrames = TRUE;
      break;
    }
  }

  // TODO: Add mutex here
  if(freeFrames == FALSE){
    if (debugflag5 && DEBUG5)
      USLOSS_Console("Pager%s(): No free frames, starting clock algorithm\n", buf);
    // If there isn't one then use clock algorithm to
    // replace a page (perhaps write to disk) 
    while(TRUE){
      // int referencebit;
      // USLOSS_MmuGetAccess(&referencebit);
      // if(!(referencebit, MMU_REF))
          // remove mutex
          // return FrameArm
      // else
          // frameTable[frameArm].referance = refbit & ~USLOSS_MMU_REF
          // USLOSS_MmuSetAccess(frame, frameTable[frameArm].referance)
      if (++frameArm > numOfFrames){
        frameArm = 0;
      }
    }
    // Setting the frame to the first unused frame the clock finds
    frame = frameArm;
    }

    //TODO:  remove mutex here
  
  return frame;

}

/*
 *----------------------------------------------------------------------
 *
 * VmInit --
 *
 * Stub for the VmInit system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is initialized.
 *
 *----------------------------------------------------------------------
 */
static void vmInit(systemArgs *sysargsPtr){

    CheckMode();

    int mappings = (long) sysargsPtr->arg1;
    int pages = (long) sysargsPtr->arg2;
    int frames = (long) sysargsPtr->arg3;
    int pagers = (long) sysargsPtr->arg4;
    int returnValue = (long) vmInitReal(mappings, pages, frames, pagers);
    if(returnValue == -1)
    {
        sysargsPtr->arg4 = (void *)((long)-1);
    }
    else
    {
        sysargsPtr->arg4 = (void *)((long)0);
    }
    sysargsPtr->arg1 = (void *) ( (long) returnValue);
    setUserMode();
} /* vmInit */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroy --
 *
 * Stub for the VmDestroy system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is cleaned up.
 *
 *----------------------------------------------------------------------
 */

static void vmDestroy(systemArgs *sysargsPtr)
{
   CheckMode();
   vmDestroyReal();
   setUserMode();
} /* vmDestroy */

/*
 *----------------------------------------------------------------------
 * Name: setUserMode
 * Purpose: switches mode from kernel mode to user mode
 * Results: None
 * Side effects: None
 *----------------------------------------------------------------------
 */
void setUserMode(){
    if(debugflag5 && DEBUG5)
        USLOSS_Console("setUserMode(): called\n");
    USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE );
}/* setUserMode */

/*
 *----------------------------------------------------------------------
 *
 * PrintStats --
 *
 *      Print out VM statistics.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Stuff is printed to the USLOSS_Console.
 *
 *----------------------------------------------------------------------
 */
void PrintStats(void){
    USLOSS_Console("VmStats\n");
    USLOSS_Console("pages:          %d\n", vmStats.pages);
    USLOSS_Console("frames:         %d\n", vmStats.frames);
    USLOSS_Console("diskBlocks:     %d\n", vmStats.diskBlocks);
    USLOSS_Console("freeFrames:     %d\n", vmStats.freeFrames);
    USLOSS_Console("freeDiskBlocks: %d\n", vmStats.freeDiskBlocks);
    USLOSS_Console("switches:       %d\n", vmStats.switches);
    USLOSS_Console("faults:         %d\n", vmStats.faults);
    USLOSS_Console("new:            %d\n", vmStats.new);
    USLOSS_Console("pageIns:        %d\n", vmStats.pageIns);
    USLOSS_Console("pageOuts:       %d\n", vmStats.pageOuts);
    USLOSS_Console("replaced:       %d\n", vmStats.replaced);
} /* PrintStats */

void initializeProcTable(){
    if(debugflag5 && DEBUG5)
        USLOSS_Console("initializeProcTable() called\n");
    int i;
    for(i = 0; i < MAXPROC; i++){
      int tmpMbox;
      Mbox_Create(1, sizeof(int), &tmpMbox);
        procTable[i] = (Process){
            .pid = -1,
            .nextProcPtr = NULL,
            .pageTable = &(PTE) {},
            // Might have to change to 1 slot later...
            .FaultMBoxID = tmpMbox,
            // MboxCreate(0,sizeof(int)),
            .pages = -1,
        //     .mboxTermReal = -1,
        //     .WakeTime = -1,
        //     .opr = 1,
        //     .track = -1,
        //     .first = -1,
        //     .sectors = -1,
        //     .buffer = (void*)(long)-1,
        };
    }
}

void addToList(int pid, procPtr *list){
    //1st Case: if the list is empty
    if(*list == NULL){
        *list = &procTable[pid%MAXPROC];
        return;
    }

    //2nd Case: add to end of list
    procPtr prev = NULL;
    procPtr curr = *list;
    // int distance = ABS(procTable[pid%MAXPROC].track, currArmTrack[unit]);
    // int tempDis = diskTrackSize[unit];
    // USLOSS_Console("adding track: %i currTrack %i\n", procTable[pid%MAXPROC].track, currArmTrack[unit]);
    while(curr != NULL){
        // tempDis = ABS(curr->track, currArmTrack[unit]);
        // USLOSS_Console("tempDis %i\n", tempDis);
        prev = curr;
        curr = curr->nextProcPtr;
    }
    prev->nextProcPtr = &procTable[pid%MAXPROC];
    procTable[pid%MAXPROC].nextProcPtr = curr;
}

void printProcList(procPtr *list){
    USLOSS_Console("PRINT THIS LIST!\n");
    procPtr curr = *list;
    while(curr != NULL){
        USLOSS_Console("\tpid %i",curr->pid);
        curr = curr->nextProcPtr;
    }
    USLOSS_Console("\n===============END=============\n");

}