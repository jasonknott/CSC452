/* ------------------------------------------------------------------------
   phase2.c

   University of Arizona
   Computer Science 452

   ------------------------------------------------------------------------ */
#include "handler.h"
#include "helper.h"
#include <usloss.h>
/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2 (char *);
static void  enableInterrupts();
static void  disableInterrupts();
static void  check_kernel_mode(char*);
int init_Slot();
/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;
int totalSlotsUsed;
// the mail boxes 
mailbox MailBoxTable[MAXMBOX];

// also need array of mail slots, array of function ptrs to system call 
// handlers, ...
struct mboxProc ProcTable[50];
struct mailSlot mailSlotTable[MAXSLOTS];
/* -------------------------- Functions ----------------------------------- */

/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg)
{
    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): at beginning\n");

    check_kernel_mode("start1");

    // Disable interrupts
    disableInterrupts();
    int kid_pid, status;
    totalSlotsUsed = 0;
    // Initialize the mail box table, slots, & other data structures.
    int i;
    for(i =0; i<MAXMBOX; i++) {
      MailBoxTable[i] = (mailbox) {
        .mboxID = -1,
        .num_slots = -1,
        .slots_size = -1,
        .slots_inuse = 0,
        .slotList = NULL,
        .mboxProcList = NULL
      };
    }

    for(i=0; i<50; i++)
    {
      ProcTable[i] = (struct mboxProc) {
        .procID = -1,
        .nextProcPtr = NULL,
        .status = -1
      };
    }

    for(i = 0; i<MAXSLOTS; i++) {
      mailSlotTable[i] = (struct mailSlot) {
        .mboxID = -1,
        .status = -1,
        .message_size = -1,
        .nextMailSlot = NULL
      };
      memset(mailSlotTable[i].message, 0, MAX_MESSAGE);
    }
    // Initialize USLOSS_IntVec and system call handlers,
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler2;
    USLOSS_IntVec[USLOSS_DISK_INT] = diskHandler;
    USLOSS_IntVec[USLOSS_TERM_INT] = termHandler;
    USLOSS_IntVec[USLOSS_SYSCALL_INT] = syscallHandler;
    // allocate mailboxes for interrupt handlers.  Etc... 
    for(i = 0; i < 7; i++){
        MboxCreate(0, 50);
    }

    enableInterrupts();

    // Create a process for start2, then block on a join until start2 quits
    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): fork'ing start2 process\n");
    kid_pid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);
    if ( join(&status) != kid_pid ) {
        USLOSS_Console("start2(): join returned something other than ");
        USLOSS_Console("start2's pid\n");
    }

    return 0;
} /* start1 */


/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailbox from the table of mailboxes and initializes it 
   Parameters - maximum number of slots in the mailbox and the max size of a msg
                sent to the mailbox.
   Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
             mailbox id.
   Side Effects - initializes one element of the mail box array. 
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size)
{
  if(DEBUG2 && debugflag2)
        USLOSS_Console("MboxCreate was called!\n");

  if(slots >= MAXSLOTS || slots < 0 || slot_size >= MAX_MESSAGE || slot_size < 0)
    return -1;

  int id = -1;
  for(int i =0; i < MAXMBOX; i++) {
    if(MailBoxTable[i].mboxID == -1) {
      id = i;
      break;
    }
  }

  if(id == -1)
    return -1;

  MailBoxTable[id] = (mailbox) {
    .mboxID = id,
    .num_slots = slots,
    .slots_size = slot_size,
    .slots_inuse = 0,
    .slotList = NULL,
    .mboxProcList = NULL
  };

  return id;
} /* MboxCreate */


/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
  if(DEBUG2 && debugflag2)
    USLOSS_Console("MboxSend(): started\n");

  //Checks for Valid Arguments
  if(msg_size >= MailBoxTable[mbox_id].slots_size || msg_size < 0 || mbox_id < 0 || mbox_id >= MAXMBOX)
    return -1;

  int slotId = init_Slot();
  // If new_slot is NULL that means we ran out of slots
   if(slotId == -1)
    return -2;
  slotPtr new_slot = &mailSlotTable[slotId];
  new_slot->slotID = slotId;
  new_slot->mboxID = mbox_id;
  new_slot->message_size = msg_size;
  new_slot->nextMailSlot = NULL;
  memset(new_slot->message, 0, MAX_MESSAGE);
  memcpy(new_slot->message, msg_ptr, msg_size);




  //Case1: take 0-Slot Inbox
  if(MailBoxTable[mbox_id].num_slots == 0) {
    if(DEBUG2 && debugflag2)
      USLOSS_Console("0 Slot Mail box used\n");
    // Is there something blocked?
    if (MailBoxTable[mbox_id].mboxProcList != NULL){
      // There is something blocked
      // Is it blocked on send or recieve
      if (MailBoxTable[mbox_id].BlockedOnSend){
        if(DEBUG2 && debugflag2)
          USLOSS_Console("The mail has processes blocked on send\n");
        // If something is blocked on a send
          if (ProcTable[getpid() % MAXPROC].procID == -1){
    // This means there is nothing in the proc table for the current proc
    if(DEBUG2 && debugflag2) 
      USLOSS_Console("MboxSend(): Adding proc to table\n");
    struct mboxProc new_proc = {
      .procID = getpid(),
      .nextProcPtr = NULL,
      .slotID = slotId
    };
    ProcTable[getpid() % MAXPROC] = new_proc;
  }
        addToBlockProcList(&MailBoxTable[mbox_id].mboxProcList, &ProcTable[getpid() % MAXPROC]);
        ProcTable[getpid() % MAXPROC].status = 12;
        blockMe(12);
        ProcTable[getpid() % MAXPROC].status = 1;
        if(MailBoxTable[mbox_id].mboxID == -1) 
          return -3;
        return 0;
      } else{
        // If something is blocked on recieve
        if(DEBUG2 && debugflag2)
          USLOSS_Console("The mail has processes blocked on send\n");
        mboxProcPtr temp_proc = MailBoxTable[mbox_id].mboxProcList;
        popMboxProcList(&MailBoxTable[mbox_id].mboxProcList);
        temp_proc->slotID = slotId;
        unblockProc(temp_proc->procID);
        return 0;
      }
    }else{
      if(DEBUG2 && debugflag2)
        USLOSS_Console("The mail has nothing blocked\n");
      // If there is nothing blocked
      MailBoxTable[mbox_id].BlockedOnSend = 1;
      // block the next thing on send
      addToBlockProcList(&MailBoxTable[mbox_id].mboxProcList, &ProcTable[getpid() % MAXPROC]);
      ProcTable[getpid() % MAXPROC].status = 12;
      blockMe(12);
      ProcTable[getpid() % MAXPROC].status = 1;
      if(MailBoxTable[mbox_id].mboxID == -1) 
        return -3;
      return 0;
    }
  }

  //Case2: Mailbox are too full
  if((MailBoxTable[mbox_id].slots_inuse >= MailBoxTable[mbox_id].num_slots)) {
    if(DEBUG2 && debugflag2) 
        USLOSS_Console("MboxSend(): Blocking process\n");
        if (ProcTable[getpid() % MAXPROC].procID == -1){
    // This means there is nothing in the proc table for the current proc
    if(DEBUG2 && debugflag2) 
      USLOSS_Console("MboxSend(): Adding proc to table\n");
    struct mboxProc new_proc = {
      .procID = getpid(),
      .nextProcPtr = NULL,
      .slotID = slotId
    };
    ProcTable[getpid() % MAXPROC] = new_proc;
  }
    addToBlockProcList(&MailBoxTable[mbox_id].mboxProcList, &ProcTable[getpid() % MAXPROC]);
    ProcTable[getpid() % MAXPROC].status = 12;
    blockMe(12);
    ProcTable[getpid() % MAXPROC].status = 1;
    if(MailBoxTable[mbox_id].mboxID == -1) 
        return -3;
    return 0;
  }

  // check if there are any blocked processes on recive in the mailbox
  if(MailBoxTable[mbox_id].mboxProcList != NULL){
    if(DEBUG2 && debugflag2) 
      USLOSS_Console("MboxSend(): Something is blocked on recive\n");
    // This means there is a blocked process
    mboxProcPtr temp_proc = MailBoxTable[mbox_id].mboxProcList;
    popMboxProcList(&MailBoxTable[mbox_id].mboxProcList);
    temp_proc->slotID = slotId;

    unblockProc(temp_proc->procID);
    return 0;
  }

  if(DEBUG2 && debugflag2) 
    USLOSS_Console("MboxSend(): Adding slot to mailbox\n");

  addToSlotList(&MailBoxTable[mbox_id].slotList, new_slot);
  MailBoxTable[mbox_id].slots_inuse++;
  totalSlotsUsed++;
  return 0;
} /* MboxSend */

/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size)
{
  if(DEBUG2 && debugflag2)
    USLOSS_Console("MboxReceive(): started\n");

  if(msg_size < 0 || mbox_id < 0 || mbox_id >= MAXMBOX)
    return -1;



  int size = -1;
  //Case 1: 0-Slot Inbox
  if (MailBoxTable[mbox_id].num_slots == 0) {
    if(DEBUG2 && debugflag2)
      USLOSS_Console("MboxReceive(): This is a 0-Slot mailbox\n");
    // If there is something blocked
    if (MailBoxTable[mbox_id].mboxProcList != NULL){
      // There is something on the block list
      // Is it blocked on send or recieve?
      if (MailBoxTable[mbox_id].BlockedOnSend){
        // Blocked on send
        if(DEBUG2 && debugflag2) 
          USLOSS_Console("MboxReceive(): Something is blocked on send\n");
        int proc_slot = ProcTable[getpid() % MAXPROC].slotID;
        if(msg_size < mailSlotTable[proc_slot].message_size) {
          memcpy(msg_ptr, mailSlotTable[proc_slot].message, msg_size);
          size = msg_size;
        } else {
          memcpy(msg_ptr, mailSlotTable[proc_slot].message, mailSlotTable[proc_slot].message_size);
          size = mailSlotTable[proc_slot].message_size;
        }
        popMboxProcList(&MailBoxTable[mbox_id].mboxProcList);
        unblockProc(MailBoxTable[mbox_id].mboxProcList->procID);
        return size;
      }else{
        if(DEBUG2 && debugflag2) 
          USLOSS_Console("MboxReceive(): Something is blocked on recieve\n");
        // Blocked on recieve
          if (ProcTable[getpid() % MAXPROC].procID == -1){
    // This means there is nothing in the proc table for the current proc
    if(DEBUG2 && debugflag2) 
      USLOSS_Console("MboxReceive(): Adding to proc table\n");
    struct mboxProc new_proc = {
      .procID = getpid(),
      .nextProcPtr = NULL,
      .slotID = -1
    };
    ProcTable[getpid() % MAXPROC] = new_proc;
  }
        addToBlockProcList(&MailBoxTable[mbox_id].mboxProcList, &ProcTable[getpid() % MAXPROC]);
        ProcTable[getpid() % MAXPROC].status = 11;
        blockMe(11);
        ProcTable[getpid() % MAXPROC].status = 1;
        if(MailBoxTable[mbox_id].mboxID == -1) {
          return -3;
        }
        int proc_slot = ProcTable[getpid() % MAXPROC].slotID;
        return mailSlotTable[proc_slot].message_size;
      }
    } else {
      if(DEBUG2 && debugflag2) 
        USLOSS_Console("MboxReceive(): Nothing is blocked, blocking on recieve\n");
      // Blocked on recieve
        if (ProcTable[getpid() % MAXPROC].procID == -1){
    // This means there is nothing in the proc table for the current proc
    if(DEBUG2 && debugflag2) 
      USLOSS_Console("MboxReceive(): Adding to proc table\n");
    struct mboxProc new_proc = {
      .procID = getpid(),
      .nextProcPtr = NULL,
      .slotID = -1
    };
    ProcTable[getpid() % MAXPROC] = new_proc;
  }
      addToBlockProcList(&MailBoxTable[mbox_id].mboxProcList, &ProcTable[getpid() % MAXPROC]);
      ProcTable[getpid() % MAXPROC].status = 11;
      blockMe(11);
      ProcTable[getpid() % MAXPROC].status = 1;
      if(MailBoxTable[mbox_id].mboxID == -1) {
        return -3;
      }
      int proc_slot = ProcTable[getpid() % MAXPROC].slotID;
      return mailSlotTable[proc_slot].message_size;
    }
  }

  //Case 2: No Slots in use
  if(MailBoxTable[mbox_id].slots_inuse == 0) {
    if(DEBUG2 && debugflag2)
      USLOSS_Console("MboxReceive(): Nothing has been sent to mailbox...Blocking\n");
      if (ProcTable[getpid() % MAXPROC].procID == -1){
    // This means there is nothing in the proc table for the current proc
    if(DEBUG2 && debugflag2) 
      USLOSS_Console("MboxReceive(): Adding to proc table\n");
    struct mboxProc new_proc = {
      .procID = getpid(),
      .nextProcPtr = NULL,
      .slotID = -1
    };
    ProcTable[getpid() % MAXPROC] = new_proc;
  }
    addToBlockProcList(&MailBoxTable[mbox_id].mboxProcList, &ProcTable[getpid() % MAXPROC]);
    ProcTable[getpid() % MAXPROC].status = 11;
    blockMe(11);
    ProcTable[getpid() % MAXPROC].status = 1;
    if(MailBoxTable[mbox_id].mboxID == -1) {
      return -3;
    }
    int proc_slot = ProcTable[getpid() % MAXPROC].slotID;
    if(msg_size < mailSlotTable[proc_slot].message_size) {
      memcpy(msg_ptr, mailSlotTable[proc_slot].message, msg_size);
      size = msg_size;
    }
    else {
      memcpy(msg_ptr, mailSlotTable[proc_slot].message, mailSlotTable[proc_slot].message_size);
      size = mailSlotTable[proc_slot].message_size;
    }
    return size;
  }

  if(DEBUG2 && debugflag2) 
    USLOSS_Console("MboxReceive(): There is a slot in the mailbox\n");


  if(msg_size < MailBoxTable[mbox_id].slotList->message_size) {
    memcpy(msg_ptr, MailBoxTable[mbox_id].slotList->message, msg_size);
    size = msg_size;
  } else {
    memcpy(msg_ptr, MailBoxTable[mbox_id].slotList->message, MailBoxTable[mbox_id].slotList->message_size);
    size = MailBoxTable[mbox_id].slotList->message_size;
  }
  
  //pop off list
  // slotInfoDump(MailBoxTable[mbox_id].slotList);
  popSlotList(&MailBoxTable[mbox_id].slotList);
  // Check is something is blocked on send (the mailbox used to be full, and now has space for waiting)
  if (MailBoxTable[mbox_id].mboxProcList != NULL){
      // This means there is things blocked on send
    int tempPID = MailBoxTable[mbox_id].mboxProcList->procID;
    USLOSS_Console("Blocked slot is %i\n", MailBoxTable[mbox_id].mboxProcList->slotID);
    addToSlotList(&MailBoxTable[mbox_id].slotList, &mailSlotTable[MailBoxTable[mbox_id].mboxProcList->slotID]);
    popMboxProcList(&MailBoxTable[mbox_id].mboxProcList);
    unblockProc(tempPID);
    } else {
      MailBoxTable[mbox_id].slots_inuse--;
      totalSlotsUsed--;
    }
  return size;
} /* MboxReceive */

  /* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - ...
   Parameters - ...
   Returns - ...
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size)
{
  if(DEBUG2 && debugflag2)
    USLOSS_Console("MboxSend(): started\n");

  //Checks for Valid Arguments
  if(msg_size >= MailBoxTable[mbox_id].slots_size || msg_size < 0 || mbox_id < 0 || mbox_id >= MAXMBOX)
    return -1;

  if(totalSlotsUsed >= MAXSLOTS)
    return -2;

  int slotId = init_Slot();
  // If new_slot is NULL that means we ran out of slots
   if(slotId == -1)
    return -2;
  slotPtr new_slot = &mailSlotTable[slotId];
  new_slot->slotID = slotId;
  new_slot->mboxID = mbox_id;
  new_slot->message_size = msg_size;
  new_slot->nextMailSlot = NULL;
  memset(new_slot->message, 0, MAX_MESSAGE);
  memcpy(new_slot->message, msg_ptr, msg_size);


  //Case1: take 0-Slot Inbox
  if(MailBoxTable[mbox_id].num_slots == 0) {
    // Is there something blocked
    if (MailBoxTable[mbox_id].mboxProcList != NULL){
    // If so
    //  
    } else {
      return -1;

    }
  }

  //Case2: Mailboxes are too full
  if((MailBoxTable[mbox_id].slots_inuse >= MailBoxTable[mbox_id].num_slots)) {
    return -2;
  }

  // check if there are any blocked processes on recive in the mailbox
  if(MailBoxTable[mbox_id].mboxProcList != NULL){
    // This means there is a blocked process
    mboxProcPtr temp_proc = MailBoxTable[mbox_id].mboxProcList;
    popMboxProcList(&MailBoxTable[mbox_id].mboxProcList);
    temp_proc->slotID = slotId;
    unblockProc(temp_proc->procID);
    return 0;
  }

  addToSlotList(&MailBoxTable[mbox_id].slotList, new_slot);
  MailBoxTable[mbox_id].slots_inuse++;
  totalSlotsUsed++;
  return 0;
} /* MboxCondSend */


/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxCondReceive(int mbox_id, void *msg_ptr, int msg_size)
{
  if(DEBUG2 && debugflag2)
    USLOSS_Console("MboxReceive(): started\n");

  if(msg_size < 0 || mbox_id < 0 || mbox_id >= MAXMBOX)
    return -1;

  if(isZapped()){
        return -3;
    }

  int size = -1;
  //Case 1: 0-Slot Inbox
  if (MailBoxTable[mbox_id].num_slots == 0) {
    return 0;
  }

  //Case 2: No Slots Available
  if(MailBoxTable[mbox_id].slots_inuse == 0) {
    return -2; //Maybe this is right 
  }
  if(msg_size < MailBoxTable[mbox_id].slotList->message_size) {
    memcpy(msg_ptr, MailBoxTable[mbox_id].slotList->message, msg_size);
    size = msg_size;
  }
  else {
    memcpy(msg_ptr, MailBoxTable[mbox_id].slotList->message, MailBoxTable[mbox_id].slotList->message_size);
    size = MailBoxTable[mbox_id].slotList->message_size;
  }
  //pop off list
  popSlotList(&MailBoxTable[mbox_id].slotList);
  // Check is something is blocked on send (the mailbox used to be full, and now has space for waiting)
  if (MailBoxTable[mbox_id].mboxProcList != NULL){
  // This means there is things blocked on send
    int tempPID = MailBoxTable[mbox_id].mboxProcList->procID;
    addToSlotList(&MailBoxTable[mbox_id].slotList, &mailSlotTable[MailBoxTable[mbox_id].mboxProcList->slotID]);
    popMboxProcList(&MailBoxTable[mbox_id].mboxProcList);
    unblockProc(tempPID);
  } else {
  // You really have to take away one slot
    MailBoxTable[mbox_id].slots_inuse--;
    totalSlotsUsed--;
  }
  return size;
} /* MboxCondReceive */

/* ------------------------------------------------------------------------
   Name - MboxRelease
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxRelease(int mbox_id)
{
  if (mbox_id < 0 || mbox_id > MAXMBOX || MailBoxTable[mbox_id].mboxID == -1){
    return -1;
  }

  mboxProcPtr temp = MailBoxTable[mbox_id].mboxProcList;
  MailBoxTable[mbox_id].mboxID = -1;
  while(temp != NULL){
    unblockProc(temp->procID);
    temp = temp->nextProcPtr;
  }

  if(isZapped()){
    return -3;
  }

  return 0;
} /* MboxRelease */

  /* ------------------------------------------------------------------------
   Name - waitDevice
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */
int waitDevice(int type, int unit, int *status)
{
  if(DEBUG2 && debugflag2) {
    USLOSS_Console("waitDevice(): started\n");
  }

  int mailbox = -1;
  if (type == USLOSS_CLOCK_INT){
    mailbox = 0;
  } else if (type == USLOSS_TERM_INT){
    mailbox = unit+1;
  } else if (type == USLOSS_DISK_INT){
    mailbox = 5 + unit;
  }
  
  if(DEBUG2 && debugflag2) {
    USLOSS_Console("MailBox it blocks at: %i\n", mailbox);
  }

  MboxReceive(mailbox, status, 0);

  if(isZapped()){
    return -1;
  }
  return 0;
} /* waitDevice */

  /* ------------------------------------------------------------------------
   Name - check_io
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */  
int check_io(){
  if(DEBUG2 && debugflag2) {
    USLOSS_Console("check_io(): started\n");
  }
  for (int i = 0; i < MAXPROC; ++i){
    if (ProcTable[i % MAXPROC].status == 11  ||
        ProcTable[i % MAXPROC].status == 12  ){
      return 1;
    }
  }
  return 0;
}/* check_io */

  /* ------------------------------------------------------------------------
   Name - enableInterrupts
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */
static void enableInterrupts()
{
  /* turn the interrupts ON iff we are in kernel mode */
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) {
        //not in kernel mode
        USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
        USLOSS_Console("enable interrupts\n");
        USLOSS_Halt(1);
    } else
        /* We ARE in kernel mode */
        USLOSS_PsrSet( USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
} /* enableInterrupts */

  /* ------------------------------------------------------------------------
   Name - disableInterrupts
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */
static void disableInterrupts()
{
  /* turn the interrupts ON iff we are in kernel mode */
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) {
        //not in kernel mode
        USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
        USLOSS_Console("enable interrupts\n");
        USLOSS_Halt(1);
    } else
        /* We ARE in kernel mode */
        USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT);
} /* disableInterrupts */

    /* ------------------------------------------------------------------------
   Name - disableInterrupts
   Purpose - 
   Parameters - 
   Returns - 
   Side Effects - none.
   ----------------------------------------------------------------------- */
static void check_kernel_mode(char* name)
{
      if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) {
        //not in kernel mode
        USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
        USLOSS_Console("check_kernel_mode\n");
        USLOSS_Halt(1);
    }
} /* check_kernel_mode */

int init_Slot() {
  for(int i = 0; i < MAXSLOTS; i++){
    if(mailSlotTable[i].mboxID == -1)
      return i;
  }
  return -1;
}
