#include "phase2.h"
#define DEBUG2 1

typedef struct mailbox   mailbox;
typedef struct mailSlot *slotPtr;
typedef struct mboxProc *mboxProcPtr;

struct mailbox {
    int       mboxID;
    int       status;
    int       slots_size;
    int       slots_inuse;
    int       num_slots;
    slotPtr   slotList;
    mboxProcPtr mboxProcList;
    // other items as needed...
};

struct mailSlot {
    int       mboxID;
    int       status;
    char      message[MAX_MESSAGE];
    int       message_size;
    slotPtr   nextMailSlot;
    // other items as needed...
};

struct mboxProc {
    int procID;
    int status;
    mboxProcPtr nextProcPtr;
    slotPtr slot;
};

struct psrBits {
    unsigned int curMode:1;
    unsigned int curIntEnable:1;
    unsigned int prevMode:1;
    unsigned int prevIntEnable:1;
    unsigned int unused:28;
};

union psrValues {
    struct psrBits bits;
    unsigned int integerPart;
};
