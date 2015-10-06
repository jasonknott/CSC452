/* Example of how to use the Unix (POSIX) fork() system call to create
 * a new processes inside a loop -- and the rapid growth in the 
 * number of processes created. Keep the loop small!! 
 * To compile:
 *    gcc -Wall -std=gnu99 -o unixfork2 unixfork2.c.
 * Then execute:
 *    unixfork2
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main (int argc, char *argv[]) {
   pid_t kidpid;
   int i;

   for (i = 0; i < 4; i++) {

      kidpid = fork();

      if (kidpid < 0) {
         fprintf(stderr, "fork failed!\n");
         exit(1);
      }

      if (kidpid == 0) {
         printf("\tI am the child, pid = %d\n", (int)getpid());
         sleep(5);
      }
      else
         printf("I’m the parent, pid = %d, my child is %d\n",
                (int)getpid(), (int)kidpid);

   }

   printf("Last printf, pid = %d\n", (int)getpid());

   return 0;
} /* main */ 
