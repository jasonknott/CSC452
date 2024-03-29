/*
 * simple2.c
 *
 */
#include <phase2.h>
#include <phase5.h>
#include <usyscall.h>
#include <libuser.h>
#include <usloss.h>
#include <string.h>

#include "test5.h"

#define TEST        "simple2"
#define PAGES       1
#define CHILDREN    1
#define FRAMES      1
#define PRIORITY    5
#define ITERATIONS  1
#define PAGERS      1
#define MAPPINGS    PAGES


void *vmRegion;

int sem;

int
Child(char *arg)
{
   int   pid;
   char  *zeros;

   GetPID(&pid);
   Tconsole("Child(): starting (pid = %d)\n", pid);

   zeros = calloc(USLOSS_MmuPageSize(), 1);
   if (memcmp(vmRegion, zeros, USLOSS_MmuPageSize()) != 0) {
      Tconsole("Child(): VmRegion is not zero-filled\n");
      abort();
   }
   verify(vmStats.faults == 1);

   SemV(sem);

   Terminate(117);
   return 0;
} /* Child */

int
start5(char *arg)
{
   int  pid;
   int  status;

   Tconsole("start5(): Running %s\n", TEST);
   Tconsole("start5(): Pagers: %d, Mappings : %d, Pages: %d, Frames: %d, Children %d, Iterations %d, Priority %d.\n", PAGERS, MAPPINGS, PAGES, FRAMES, CHILDREN, ITERATIONS, PRIORITY);

   vmRegion = VmInit( MAPPINGS, PAGES, FRAMES, PAGERS );

   Spawn("Child", Child,  0,USLOSS_MIN_STACK*7,PRIORITY, &pid);
   SemP( sem);
   Wait(&pid, &status);
   verify(status == 117);

   Tconsole("start5 done\n");
   //PrintStats();
   VmDestroy();
   Terminate(1);

   return 0;
} /* start5 */
