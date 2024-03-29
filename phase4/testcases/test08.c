#include <stdio.h>
#include <usloss.h>
#include <libuser.h>
#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <usyscall.h>
#include <strings.h>


/* Queries each of disk0 and disk1 to get the sector size, track size,
 * and disk size.
 */

int start4(char *arg)
{
    int unit, sectorSize, trackSize, diskSize;

    USLOSS_Console("start4(): started\n");

    unit = 0;
    DiskSize(unit, &sectorSize, &trackSize, &diskSize);

    USLOSS_Console("start4(): unit %d, sector size %d, track size %d, ",
                   unit, sectorSize, trackSize);
    USLOSS_Console("disk size %d\n", diskSize);

    unit = 1;
    DiskSize(unit, &sectorSize, &trackSize, &diskSize);

    USLOSS_Console("start4(): unit %d, sector size %d, track size %d, ",
                   unit, sectorSize, trackSize);
    USLOSS_Console("disk size %d\n", diskSize);

    USLOSS_Console("start4(): calling Terminate\n");
    Terminate(0);

    USLOSS_Console("start4(): should not see this message!\n");
    return 0;

} /* start4 */
