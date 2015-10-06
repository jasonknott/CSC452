/* ------------------------- Library -------------------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <usloss.h>
#include <string.h>
#include "message.h"
/* ------------------------- Prototypes ----------------------------------- */
void addToSlotList(slotPtr *, slotPtr);
void addToBlockProcList(mboxProcPtr *, mboxProcPtr);
void popSlotList(slotPtr*);
void popMboxProcList(mboxProcPtr*);
void slotInfoDump(slotPtr);
/* ------------------------------------------------------------------------ */