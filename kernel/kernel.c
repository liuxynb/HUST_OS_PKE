/*
 * Supervisor-mode startup codes
 */

#include "riscv.h"
#include "string.h"
#include "elf.h"
#include "process.h"

#include "spike_interface/spike_utils.h"

// process is a structure defined in kernel/process.h
// modified on lab1_c3
process user_app[NCPU];

// load the elf, and construct a "process" (with only a trapframe).
// load_bincode_from_host_elf is defined in elf.c
void load_user_program(process *proc) {
  int hartid = read_tp();
  // USER_TRAP_FRAME is a physical address defined in kernel/config.h
  proc->trapframe = (trapframe *)(uint64)USER_TRAP_FRAME(hartid);
  memset(proc->trapframe, 0, sizeof(trapframe));
  // USER_KSTACK is also a physical address defined in kernel/config.h
  proc->kstack = USER_KSTACK(hartid);
  proc->trapframe->regs.sp = USER_STACK(hartid);
  proc->trapframe->regs.tp = hartid; // set the thread pointer tp to hartid

  // load_bincode_from_host_elf() is defined in kernel/elf.c
  load_bincode_from_host_elf(proc);
}

//
// s_start: S-mode entry point of riscv-pke OS kernel.
//
int s_start(void) {
  int hartid = read_tp();
  sprint("hartid = %d: Enter supervisor mode...\n", hartid);
  // Note: we use direct (i.e., Bare mode) for memory mapping in lab1.
  // which means: Virtual Address = Physical Address
  // therefore, we need to set satp to be 0 for now. we will enable paging in lab2_x.
  // 
  // write_csr is a macro defined in kernel/riscv.h
  write_csr(satp, 0);

  // the application code (elf) is first loaded into memory, and then put into execution
  load_user_program(&user_app[hartid]);

  sprint("hartid = %d: Switch to user mode...\n", hartid);
  // switch_to() is defined in kernel/process.c
  switch_to(&user_app[hartid]);

  // we should never reach here.
  return 0;
}
