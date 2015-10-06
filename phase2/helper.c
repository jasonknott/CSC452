#include "helper.h"
extern int debugflag2;
/* ------------------------------------------------------------------------
 *    Name - addToSlotList
 *    Purpose - ...
 *    Parameters - ..
 *    Returns - ../
 *    Side Effects - ..
 *  ----------------------------------------------------------------------- */
void addToSlotList(slotPtr* slotList, slotPtr new_slot){
  // A simple add to end of list function
  if(DEBUG2 && debugflag2) {
    USLOSS_Console("addToSlotList(): started\n");
  }

  if (*slotList == NULL){
    *slotList = new_slot;
    return;
  }

    slotPtr temp = *slotList; //might need a star here

  // Gets to last element
  while(temp->nextMailSlot != NULL){
      temp = temp->nextMailSlot;
  }

  temp->nextMailSlot = new_slot;
  new_slot->nextMailSlot = NULL;
  slotInfoDump(*slotList);
}/* addToSlotList */

/* ------------------------------------------------------------------------
 *    Name - ...
 *    Purpose - ...
 *    Parameters - ..
 *    Returns - ../
 *    Side Effects - ..
 *  ----------------------------------------------------------------------- */
void popSlotList(slotPtr* slotList) {
    if(*slotList == NULL)
        return;
    slotPtr temp = *slotList;
    *slotList = temp->nextMailSlot;
    temp->mboxID = -1;
    temp->slotID = -1;
    temp->status = -1;
    memset(temp->message, 0, MAX_MESSAGE);;
    temp->message_size = -1;
    temp->nextMailSlot = NULL;
} /* popSlotList */

/* ------------------------------------------------------------------------
 *    Name - ...
 *    Purpose - ...
 *    Parameters - ..
 *    Returns - ../
 *    Side Effects - ..
 *  ----------------------------------------------------------------------- */
void addToBlockProcList(mboxProcPtr* list, mboxProcPtr proc){
  // A simple add to end of list function
  if(DEBUG2 && debugflag2) {
    USLOSS_Console("addToBlockProcList(): started\n");
  }
  if (*list == NULL){
    *list = proc;
    return;
  }
  // blockListInfoDump(list);
  // USLOSS_Console("addToBlockProcList(): Looking inside list\n");
  mboxProcPtr temp_proc = *list; //might need a star here

  // Gets to last element
  while(temp_proc->nextProcPtr != NULL){
      temp_proc= temp_proc->nextProcPtr;
  }
  temp_proc->nextProcPtr = proc;
}

/* ------------------------------------------------------------------------
 *    Name - ...
 *    Purpose - ...
 *    Parameters - ..
 *    Returns - ../
 *    Side Effects - ..
 *  ----------------------------------------------------------------------- */
 void popMboxProcList(mboxProcPtr* procList) {
  if(*procList == NULL)
      return;
  mboxProcPtr temp = *procList;
  *procList = temp->nextProcPtr;
}/* popMboxProcList */

/* ------------------------------- DEBUG ---------------------------------- */

/* ------------------------------------------------------------------------
 *    Name - ...
 *    Purpose - ...
 *    Parameters - ..
 *    Returns - ../
 *    Side Effects - ..
 *  ----------------------------------------------------------------------- */
void blockListInfoDump(mboxProcPtr list){
    int i = 0;
    USLOSS_Console("=======START BLOCK INFO DUMP=======\n");
    while(list != NULL ){
      USLOSS_Console("\tBlock # %i\n", i++ );
      USLOSS_Console("\tBlock PID: %i\n", list->procID );
      USLOSS_Console("\tnextProcPnt: %i\n", list->nextProcPtr );
      USLOSS_Console("\n");
      list = list->nextProcPtr;
    }
    USLOSS_Console("=======END BLOCK INFO DUMP=======\n");  
}/* blockListInfoDump */

/* ------------------------------------------------------------------------
 *    Name - ...
 *    Purpose - ...
 *    Parameters - ..
 *    Returns - ../
 *    Side Effects - ..
 *  ----------------------------------------------------------------------- */
void slotInfoDump(slotPtr slot){
    int i = 0;
    USLOSS_Console("=======START SLOT INFO DUMP=======\n");
    while(slot != NULL ){
      USLOSS_Console("\tSlot # %i\n", i++ );
      USLOSS_Console("\tSlotId: %i\n", slot->slotID);
      USLOSS_Console("\tSlot message: %s\n", slot->message);
      USLOSS_Console("\tSlot message size: %i\n", slot->message_size);
      USLOSS_Console("\tSlot nextMailSlot: 0x%d\n", slot->nextMailSlot);
      USLOSS_Console("\n");
      slot = slot->nextMailSlot;
    }
    USLOSS_Console("=======END SLOT INFO DUMP=======\n");
}/* slotInfoDump */

