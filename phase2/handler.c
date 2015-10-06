#include "handler.h"

extern int debugflag2;

/* an error method to handle invalid syscalls */
void nullsys(sysargs *args)
{
    USLOSS_Console("nullsys(): Invalid syscall. Halting...\n");
    USLOSS_Halt(1);
} /* nullsys */


void clockHandler2(int dev, void *arg)
{

  if (DEBUG2 && debugflag2)
    USLOSS_Console("clockHandler2(): called\n");
  static int i = 1;
  if (i++ > 4){
    i = 0;
    MboxCondSend(0, 0, 0);
  }
  timeSlice();
} /* clockHandler */


void diskHandler(int dev, void *arg)
{
  if (DEBUG2 && debugflag2)
      USLOSS_Console("diskHandler(): called\n");
  int status=0;
  USLOSS_DeviceInput(dev, (int) arg, &status);
  MboxCondSend(5 + (int) arg, &status, sizeof(status));
} /* diskHandler */


void termHandler(int dev, void *arg)
{

  if (DEBUG2 && debugflag2)
      USLOSS_Console("termHandler(): called\n");
  int status = 0;
  USLOSS_DeviceInput(dev, (int) arg, &status);
  MboxCondSend((int) arg+1, &status, sizeof(status));


} /* termHandler */


void syscallHandler(int dev, void *arg)
{
   if (DEBUG2 && debugflag2)
      USLOSS_Console("syscallHandler(): called\n");
} /* syscallHandler */