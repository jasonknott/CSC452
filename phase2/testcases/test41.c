/* Test of conditional send and conditional receive that does not rely
 * on a zero-slot mailbox.  Similar to test12.c, but uses a 1-slot mailbox
 * called "pause_mbox" instead of the 0-slot private_mbox.
 */

#include <stdio.h>
#include <strings.h>
 #include <string.h>
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>

int XXp1(char *);
int XXp3(char *);
char buf[256];

int mbox_id, pause_mbox;

int start2(char *arg)
{
    int kid_status, kidpid;

    USLOSS_Console("start2(): started\n");
    mbox_id = MboxCreate(5, 50);
    USLOSS_Console("start2(): MboxCreate returned id = %d\n", mbox_id);
    pause_mbox = MboxCreate(1, 50);
    USLOSS_Console("start2(): MboxCreate returned id = %d\n", pause_mbox);

    kidpid = fork1("XXp1",  XXp1, NULL,    2 * USLOSS_MIN_STACK, 3);
    kidpid = fork1("XXp3",  XXp3, NULL,    2 * USLOSS_MIN_STACK, 4);

    kidpid = join(&kid_status);
    USLOSS_Console("\nstart2(): joined with kid %d, status = %d\n",
                   kidpid, kid_status);

    kidpid = join(&kid_status);
    USLOSS_Console("\nstart2(): joined with kid %d, status = %d\n",
                   kidpid, kid_status);

    quit(0);
    return 0; /* so gcc will not complain about its absence... */
} /* start2 */


int XXp1(char *arg)
{
    int i, result;
    char buffer[20];
    int  msgNum = 0;

    USLOSS_Console("\nXXp1(): started\n");

    for (i = 0; i < 8; i++) {
        USLOSS_Console("\nXXp1(): conditionally sending message #%d ", msgNum);
        USLOSS_Console("to mailbox %d\n", mbox_id);
        sprintf(buffer, "hello there, #%d", msgNum);
        result = MboxCondSend(mbox_id, buffer, strlen(buffer)+1);
        USLOSS_Console("XXp1(): after conditional send of message ");
        USLOSS_Console("#%d, result = %d\n", msgNum, result);
        msgNum++;
    }

    MboxReceive(pause_mbox, NULL, 0); // should block on mail box

    for (i = 0; i < 8; i++) {
        USLOSS_Console("\nXXp1(): conditionally sending message #%d ", msgNum);
        USLOSS_Console("to mailbox %d\n", mbox_id);
        sprintf(buffer, "good-bye, #%d", i);
        result = MboxCondSend(mbox_id, buffer, strlen(buffer)+1);
        USLOSS_Console("XXp1(): after conditional send of message ");
        USLOSS_Console("#%d, result = %d\n", msgNum, result);
        msgNum++;
    }

    quit(-3);
    return 0; /* so gcc will not complain about its absence... */
} /* XXp1 */


int XXp3(char *arg)
{
    char buffer[100];
    int i = 0, result, count;

    USLOSS_Console("\nXXp3(): started\n");

    count = 0;
    while ( (result = MboxCondReceive(mbox_id, buffer, 100)) >= 0 ) {
        USLOSS_Console("XXp3(): conditionally received message #%d ", i);
        USLOSS_Console("from mailbox %d\n", mbox_id);
        USLOSS_Console("        message = `%s'\n", buffer);
        count++;
    }
    USLOSS_Console("XXp3(): After loop, result is negative; result = %d\n",
                   result);
    USLOSS_Console("XXp3(): received %d hello messages from mailbox %d\n",
                   count, mbox_id);

    MboxSend(pause_mbox, NULL, 0); // should release XXp1

    count = 0;
    while ( (result = MboxCondReceive(mbox_id, buffer, 100)) >= 0 ) {
        USLOSS_Console("XXp3(): conditionally received message #%d ", i);
        USLOSS_Console("from mailbox %d\n", mbox_id);
        USLOSS_Console("        message = `%s', result = %d\n", buffer, result);
        count++;
    }
    USLOSS_Console("XXp3(): After loop, result is negative; result = %d\n",
                   result);
    USLOSS_Console("XXp3(): received %d good-bye messages from mailbox %d\n",
                   count, mbox_id);

    quit(-4);
    return 0;
} /* XXp3 */
