/*
 * This app fork a child process to read and write the heap data from parent process.
 * Because implemented copy on write, when child process only read the heap data,
 * the physical address is the same as the parent process.
 * But after writing, child process heap will have different physical address.
 */

#include "user/user_lib.h"
#include "util/types.h"

int main(void)
{
  int *heap_data = (int *)malloc(128);
  printu("the physical address of parent process heap is: ");
  printpa(heap_data);
  int pid = fork();
  if (pid == 0)
  {
    printu("the physical address of child process heap before copy on write is: ");
    printpa(heap_data);
    heap_data[0] = 0;
    printu("the physical address of child process heap after copy on write is: ");
    printpa(heap_data);
  }
  exit(0);
  return 0;
}