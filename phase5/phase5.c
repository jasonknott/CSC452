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

#define NUMTRACKS 16

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
int findFrame(int);
int getFrame(int, int, void*);
int outputFrame(int, int, void*);

// Globals
int sector;
int track;
int disk;
int faultMailbox;
VmStats  vmStats;
static Process procTable[MAXPROC];
static trackBlock trackBlockTable[NUMTRACKS];
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
	for(int i = 0; i< NUMTRACKS; i++)
	{
		trackBlockTable[i].page = -1;
		trackBlockTable[i].pid = -1;
		trackBlockTable[i].frame = -1;
		trackBlockTable[i].status = UNUSED;
	}
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
    
    vmStarted = TRUE;


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
        procTable[i].numPages = pages;
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
      int pid = fork1("Pager", Pager, &str[0], USLOSS_MIN_STACK, 2);

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


    USLOSS_Console("before: vmRegion\n");
    //Filling vmRegion with 0's
    // DON'T MOVE!!! This has black magic in it, and it scares me....
    memset(vmRegion, 0, (USLOSS_MmuPageSize() * pages));
    USLOSS_Console("after: vmRegion\n");

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
  int pagerID = atoi(buf);
  if (debugflag5 && DEBUG5){
      USLOSS_Console("Pager%s(): started\n", buf);
  }
  FaultMsg fault;  
  void *bufDisk = malloc(USLOSS_MmuPageSize());

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

    int frame = findFrame(pagerID);
    if (debugflag5 && DEBUG5)
      USLOSS_Console("Pager%s(): Frame #%i found\n", buf, frame);

    procTable[pid % MAXPROC].pageTable[page].frame = frame;

    if(frameTable[frame].state == UNUSED){
      if (debugflag5 && DEBUG5)
        USLOSS_Console("Pager%s(): Frame #%i is unused\n", buf, frame);
      // vmStats.freeFrames--;
      vmStats.new++;
    } else{
        // The frame is used...
        // get access with USELOSS call
        int refBit; 
        // int USLOSS_MmuGetAccess(int frame, int *accessPtr);
        USLOSS_MmuGetAccess(frame, &refBit);
        if (refBit & USLOSS_MMU_DIRTY){
              int trackBlock = outputFrame(pid, page, &bufDisk);
              // store diskBlock in old pagetable Entry
              procTable[pid % MAXPROC].pageTable[page].trackBlock = trackBlock;
              frameTable[trackBlockTable[trackBlock].frame].pid = pid;
        } 

        if (procTable[pid % MAXPROC].pageTable[page].state == UNUSED){
              // This is the easy case of it not being on the disk
              // Page might be 0 because we map it for page?
              USLOSS_MmuMap(0, 0, frame, USLOSS_MMU_PROT_RW);
              memset(vmRegion, '\0', USLOSS_MmuPageSize());
        } else {
              // If the frame is on disk, get the data and copy it too the vmRegion
              int oldFrame = getFrame(pid, page, bufDisk);
              USLOSS_MmuMap(0, 0, oldFrame, USLOSS_MMU_PROT_RW);
              memcpy(vmRegion, bufDisk, USLOSS_MmuPageSize());
        }
    }

    // USLOSS_mmu_setaccess(frame, 0);
    // USLOSS_Mmu_UnMap(0, 0, frame, USLOSS_MMU_PROT_RW)
   if (debugflag5 && DEBUG5)
      USLOSS_Console("Pager%s(): About to call USLOSS_MmuMap\n", buf);
    // USLOSS_MmuMap(int tag, int page, int frame, int protection);
    int result = USLOSS_MmuMap(0, page, frame, USLOSS_MMU_PROT_RW);
    if (result != USLOSS_MMU_OK) {
      USLOSS_Console("process %d: Pager failed mapping: %d\n", getpid(), result);
      USLOSS_Halt(1);
    }



    // cleaning up the frametable
    frameTable[frame].pid = pid;
    frameTable[frame].clean = TRUE;
    frameTable[frame].referenced = TRUE;
    frameTable[frame].page = page;
    
    /* Unblock waiting (faulting) process */
    result = MboxSend(fault.replyMbox, "", 0);
    if (result < 0) {
      USLOSS_Console("There bas been an error\n");
    }

  }
  quit(0);
  return 0;
} /* Pager */

/*
 *----------------------------------------------------------------------
 *
 * getFrame --
 *
 * 
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      
 *
 *----------------------------------------------------------------------
 */

int getFrame(int pid,int page,void *bufDisk){
  // get diskblock from disk
  int track;
  int frame;
  for(int i = 0; i < NUMTRACKS; i++)
  {
	if(trackBlockTable[i].pid == pid && trackBlockTable[i].page == page)
	{
		track = i;
		frame = trackBlockTable[i].frame;
		break;
	}
  }
  //int diskblock = procTable[pid].pageTable[page].diskBlock;
  diskReadReal(1, track, 0, 8, bufDisk);
  vmStats.pageIns++;
  return frame;
}

int outputFrame(int pid, int pagenum, void *bufDisk)
{
  USLOSS_MmuMap(0, pagenum, procTable[pid%MAXPROC].pageTable[pagenum].frame, USLOSS_MMU_PROT_RW);
  memcpy(bufDisk, vmRegion, USLOSS_MmuPageSize());
  int track = -1;
  for(int i = 0; i < NUMTRACKS; i++)
  {
	if(trackBlockTable[i].status == UNUSED)
	{
		track = i; 
		break;
	}
  }
  if(track == -1)
  {
	USLOSS_Console("Disk is FULL!\n");
	USLOSS_Halt(1);
  }
  diskWriteReal(1, track, 0, 8, bufDisk); 
  trackBlockTable[track].status = INCORE; 
  vmStats.pageOuts++;
  return track;
}

/*
 *----------------------------------------------------------------------
 *
 * fork1 --
 *
 * 
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      
 *
 *----------------------------------------------------------------------
 */

int findFrame(int pagerID) {
  if (debugflag5 && DEBUG5)
    USLOSS_Console("findFrame%i(): started\n", pagerID);

  /* Look for free frame */
  int frame = 0;
  int freeFrames = vmStats.freeFrames != 0;
  for (frame = 0; frame < numOfFrames && freeFrames; ++frame){
    if(frameTable[frame].state == UNUSED){
      freeFrames = TRUE;
      break;
    }
  }

  // TODO: Add mutex here
  if(freeFrames == FALSE)
  {
	if (debugflag5 && DEBUG5)
	{
		USLOSS_Console("Pager%i(): No free frames, starting clock algorithm\n", pagerID);
	}
    // If there isn't one then use clock algorithm to
    // replace a page (perhaps write to disk) 
    while(TRUE)
	{
		int referencebit;
		USLOSS_MmuGetAccess(frameArm, &referencebit);
		if(!(referencebit & USLOSS_MMU_REF))
		{
			//remove mutex
			return frameArm;
		}
		else
		{
			frameTable[frameArm].referenced = referencebit & ~ USLOSS_MMU_REF;
			USLOSS_MmuSetAccess(frameArm, frameTable[frameArm].referenced);
		}
		if (++frameArm > numOfFrames)
		{
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
 * fork1 --
 *
 * 
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      
 *
 *----------------------------------------------------------------------
 */
void forkReal(int pid){
  if(debugflag5 && DEBUG5)
    USLOSS_Console("forkReal(): forking %i\n", pid);
  if(vmStarted == FALSE && pid != 9)
  {
    return;
  }
  //There is nothing to do, because we do this in vmINIT; which is VERY BAD!
  int numPages = procTable[pid%MAXPROC].numPages;
  procTable[pid % MAXPROC].pageTable = malloc(sizeof(PTE) * numPages);
  if(debugflag5 && DEBUG5)
    USLOSS_Console("forkReal(): pageTable %x\n", procTable[pid % MAXPROC].pageTable);

  for(int i = 0; i < numPages; i++)
  {
    procTable[pid % MAXPROC].pageTable[i].state = UNUSED;
    procTable[pid % MAXPROC].pageTable[i].frame = -1;
    procTable[pid % MAXPROC].pageTable[i].trackBlock = -1;
    //Add more stuff here. 
  }
  procTable[pid % MAXPROC].pid = pid;
  
}/* fork1 */

/*
 *----------------------------------------------------------------------
 *
 * switch1 --
 *
 * switch out pages from the old to new 
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      
 *
 *----------------------------------------------------------------------
 */
void switchReal(int old, int new)
{
	if(vmStarted == FALSE)
	{
		return;
	}
	int status;
	vmStats.switches++;
	int numPages = sizeof(procTable[old%MAXPROC].pageTable)/sizeof(PTE);
	for(int i = 0; i < numPages; i++)
	{
		int frame = procTable[old%MAXPROC].pageTable[i].frame;
		if(frame != -1)
		{
			status = USLOSS_MmuUnmap(0, i);
			if(status != USLOSS_MMU_OK)
			{
				USLOSS_Console("Failed to do UnMap on Switch: \n\t old_pid=%d\n", old);
				USLOSS_Halt(1);
			}	
		}
	}

	for(int i = 0; i < numPages; i++)
	{
		if(procTable[new%MAXPROC].pageTable[i].frame != -1)
		{
			status = USLOSS_MmuMap(0, i, procTable[new%MAXPROC].pageTable[i].frame, USLOSS_MMU_PROT_RW);
			if(status != USLOSS_MMU_OK)
			{
				USLOSS_Console("Failed to do Map on Switch: \n\t new_pid=%d\n", new);
				USLOSS_Halt(1);
			}
		}
	}
}/* switch1 */

/*
 *----------------------------------------------------------------------
 *
 * quit1 --
 *
 * quits the process and cleans up  
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      
 *
 *----------------------------------------------------------------------
 */
void quitReal(int pid)
{
	if(vmStarted == FALSE)
	{
		return;
	}
	int status;
	int numPages = sizeof(procTable[pid % MAXPROC].pageTable)/sizeof(PTE);
	for(int i = 0; i < numPages; i++)
	{
		int frame = procTable[pid % MAXPROC].pageTable[i].frame;
		if(frame != -1)
		{
			status = USLOSS_MmuUnmap(i, frame);
			if(status != USLOSS_MMU_OK)
			{
				USLOSS_Console("Failed to do UnMap on Quit: \n\t pid=%d\n", pid);
				USLOSS_Halt(1);
			}
			procTable[pid%MAXPROC].pageTable[i].state = UNUSED;
			procTable[pid%MAXPROC].pageTable[i].frame = -1;
			procTable[pid%MAXPROC].pageTable[i].trackBlock = -1; //Maybe?
			frameTable[frame].pid = -1;
			frameTable[frame].referenced = FALSE; //This may not allow track to be reused.
			//TODO: NEED TO ADD MORE HERE
			vmStats.freeFrames++;
		}
	}
	//TODO: NEED TO DO DiskBlock[] HERE
	for(int i = 0; i < NUMTRACKS; i++)
	{
		if(trackBlockTable[i].pid == pid)
		{
			trackBlockTable[i].status = UNUSED;
		}
	}
	procTable[pid % MAXPROC].pid = -1;
	free(procTable[pid%MAXPROC].pageTable); //WARNING: MAY CAUSE ERRORS!
	procTable[pid%MAXPROC].pageTable= NULL;
}/* quit1 */
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
            .pageTable = NULL,
			      .numPages = 10,
            .FaultMBoxID = tmpMbox,
            .pages = -1,
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
