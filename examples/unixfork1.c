/* Example of how to use the Unix (POSIX) fork() system call to create
 * new processes.
 * To compile (assuming the file is named "unixfork1.c"):
 *    gcc -Wall -std=gnu99 -o unixfork1 unixfork1.c
 * Then execute:
 *    unixfork1
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main (int argc, char *argv[]) {
   pid_t kidpid;

   kidpid = fork();

   /* both parent and child execute this if statement! */
   if (kidpid < 0) {
      fprintf(stderr, "fork failed!\n");
      exit(1);
   }

   /* both parent and child ask this, get different answers! */
   if (kidpid == 0) {
      printf("I am the child, pid = %d\n", (int)getpid()); }
   else {
      printf("I am the parent, pid = %d, my child is %d\n",
             (int)getpid(), kidpid); }

   /* both parent and child execute this */
   printf("Last printf, pid = %d\n", (int)getpid());

   return 0;
} /* main */
