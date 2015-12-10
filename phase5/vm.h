/*
 * vm.h
 */


/*
 * All processes use the same tag.
 */
#define TAG 0

/*
 * Different states for a page.
 */
#define UNUSED 500
#define INCORE 501
#define USED   502
/* You'll probably want more states */


/*
 * Page table entry.
 */
typedef struct PTE {
    int  state;      // See above.
    int  frame;      // Frame that stores the page (if any). -1 if none.
    int  trackBlock;  // Disk block that stores the page (if any). -1 if none.
    // Add more stuff here
} PTE;

/*
 * Frame table entry.
 */
typedef struct FTE {
    int  pid;  
    int  referenced;
    int  clean;
    int  state;      // See above.
    int  page;
    // Add more stuff here
} FTE;

typedef struct trackBlock {
	int page;
	int pid;
	int frame;
	int status;
} trackBlock;



/*
 * Per-process information.
 */
typedef struct Process * procPtr;

typedef struct Process {
    int pid;
    procPtr nextProcPtr;
    int  numPages;   // Size of the page table.
    PTE  *pageTable; // The page table for the process.
    int FaultMBoxID;
    int pages;
    // Add more stuff here */
} Process;

/*
 * Information about page faults. This message is sent by the faulting
 * process to the pager to request that the fault be handled.
 */
typedef struct FaultMsg {
    int  pid;        // Process with the problem.
    void *addr;      // Address that caused the fault.
    int  replyMbox;  // Mailbox to send reply.
    // Add more stuff here.
} FaultMsg;

#define CheckMode() assert(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)
