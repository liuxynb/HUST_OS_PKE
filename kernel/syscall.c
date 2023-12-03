/*
 * contains the implementation of all syscalls.
 */

#include <stdint.h>
#include <errno.h>

#include "util/types.h"
#include "syscall.h"
#include "string.h"
#include "process.h"
#include "util/functions.h"
#include "elf.h"
#include "spike_interface/spike_utils.h"

//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char *buf, size_t n)
{
  sprint(buf);
  return 0;
}

//
// implement the SYS_user_exit syscall
//
ssize_t sys_user_exit(uint64 code)
{
  sprint("User exit with code:%d.\n", code);
  // in lab1, PKE considers only one app (one process).
  // therefore, shutdown the system when the app calls exit()
  shutdown(code);
}

// added on @lab1_challenge1
// implement the SYS_user_backtrace syscall

ssize_t sys_user_backtrace(uint64 trace_depth)
{
  uint64 user_sp = current->trapframe->regs.sp + 16;
  uint64 current_p = user_sp;
  for (int current_depth = 0; current_depth < trace_depth;)
  {
    // sprint("%d\n ", current_depth);
    uint64 current_ra = *(uint64 *)(current_p + 8);
    uint64 current_fp = *(uint64 *)(current_p);
    if (current_ra == 0)
    {
      sprint("reach the bottom of the stack\n");
      break; // * 到达用户栈底
    }
    // sprint("backtrace %d: ra = %p, fp = %p\n", current_depth, current_ra, current_fp);
    // sprint("0x%x\n", current_ra);
    if (elf_print_name(current_ra, &current_depth, trace_depth) == EL_ERR)
    {
      panic("backtrace %d: ra = %p, fp = %p\n", current_depth, current_ra, current_fp);
    }
    current_p += 16;
  }
  return 0;
}

//
// [a0]: the syscall number; [a1] ... [a7]: arguments to the syscalls.
// returns the code of success, (e.g., 0 means success, fail for otherwise)
//
long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7)
{
  switch (a0)
  {
  case SYS_user_print:
    return sys_user_print((const char *)a1, a2);
  case SYS_user_exit:
    return sys_user_exit(a1);
  case SYS_user_backtrace:
    return sys_user_backtrace(a1);
  default:
    panic("Unknown syscall %ld \n", a0);
  }
}
