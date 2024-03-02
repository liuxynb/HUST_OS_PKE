/*
 * Utility functions for process management.
 *
 * Note: in Lab1, only one process (i.e., our user application) exists. Therefore,
 * PKE OS at this stage will set "current" to the loaded user application, and also
 * switch to the old "current" process after trap handling.
 */

#include "riscv.h"
#include "strap.h"
#include "config.h"
#include "process.h"
#include "elf.h"
#include "string.h"
#include "vmm.h"
#include "pmm.h"
#include "memlayout.h"
#include "sched.h"
#include "spike_interface/spike_utils.h"
#include <stdbool.h>

// Two functions defined in kernel/usertrap.S
extern char smode_trap_vector[];
extern void return_to_user(trapframe *, uint64 satp);

// trap_sec_start points to the beginning of S-mode trap segment (i.e., the entry point
// of S-mode trap vector).
extern char trap_sec_start[];

// process pool. added @lab3_1
process procs[NPROC];

// current points to the currently running user-mode application.
process *current = NULL;

//
// switch to a user-mode process
//
void switch_to(process *proc)
{
  assert(proc);
  current = proc;

  // write the smode_trap_vector (64-bit func. address) defined in kernel/strap_vector.S
  // to the stvec privilege register, such that trap handler pointed by smode_trap_vector
  // will be triggered when an interrupt occurs in S mode.
  write_csr(stvec, (uint64)smode_trap_vector);

  // set up trapframe values (in process structure) that smode_trap_vector will need when
  // the process next re-enters the kernel.
  proc->trapframe->kernel_sp = proc->kstack;     // process's kernel stack
  proc->trapframe->kernel_satp = read_csr(satp); // kernel page table
  proc->trapframe->kernel_trap = (uint64)smode_trap_handler;

  // SSTATUS_SPP and SSTATUS_SPIE are defined in kernel/riscv.h
  // set S Previous Privilege mode (the SSTATUS_SPP bit in sstatus register) to User mode.
  unsigned long x = read_csr(sstatus);
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode

  // write x back to 'sstatus' register to enable interrupts, and sret destination mode.
  write_csr(sstatus, x);

  // set S Exception Program Counter (sepc register) to the elf entry pc.
  write_csr(sepc, proc->trapframe->epc);

  // make user page table. macro MAKE_SATP is defined in kernel/riscv.h. added @lab2_1
  uint64 user_satp = MAKE_SATP(proc->pagetable);

  // return_to_user() is defined in kernel/strap_vector.S. switch to user mode with sret.
  // note, return_to_user takes two parameters @ and after lab2_1.
  return_to_user(proc->trapframe, user_satp);
}

//
// initialize process pool (the procs[] array). added @lab3_1
//
void init_proc_pool()
{
  memset(procs, 0, sizeof(process) * NPROC);

  for (int i = 0; i < NPROC; ++i)
  {
    procs[i].status = FREE;
    procs[i].pid = i;
  }
}

//
// allocate an empty process, init its vm space. returns the pointer to
// process strcuture. added @lab3_1
//
process *alloc_process()
{
  // locate the first usable process structure
  int i;

  for (i = 0; i < NPROC; i++)
    if (procs[i].status == FREE)
      break;

  if (i >= NPROC)
  {
    panic("cannot find any free process structure.\n");
    return 0;
  }

  // init proc[i]'s vm space
  procs[i].trapframe = (trapframe *)alloc_page(); // trapframe, used to save context
  memset(procs[i].trapframe, 0, sizeof(trapframe));

  // page directory
  procs[i].pagetable = (pagetable_t)alloc_page();
  memset((void *)procs[i].pagetable, 0, PGSIZE);

  procs[i].kstack = (uint64)alloc_page() + PGSIZE; // user kernel stack top
  uint64 user_stack = (uint64)alloc_page();        // phisical address of user stack bottom
  procs[i].trapframe->regs.sp = USER_STACK_TOP;    // virtual address of user stack top

  // allocates a page to record memory regions (segments)
  procs[i].mapped_info = (mapped_region *)alloc_page();
  memset(procs[i].mapped_info, 0, PGSIZE);

  // map user stack in userspace
  user_vm_map((pagetable_t)procs[i].pagetable, USER_STACK_TOP - PGSIZE, PGSIZE,
              user_stack, prot_to_type(PROT_WRITE | PROT_READ, 1));
  procs[i].mapped_info[STACK_SEGMENT].va = USER_STACK_TOP - PGSIZE;
  procs[i].mapped_info[STACK_SEGMENT].npages = 1;
  procs[i].mapped_info[STACK_SEGMENT].seg_type = STACK_SEGMENT;

  // map trapframe in user space (direct mapping as in kernel space).
  user_vm_map((pagetable_t)procs[i].pagetable, (uint64)procs[i].trapframe, PGSIZE,
              (uint64)procs[i].trapframe, prot_to_type(PROT_WRITE | PROT_READ, 0));
  procs[i].mapped_info[CONTEXT_SEGMENT].va = (uint64)procs[i].trapframe;
  procs[i].mapped_info[CONTEXT_SEGMENT].npages = 1;
  procs[i].mapped_info[CONTEXT_SEGMENT].seg_type = CONTEXT_SEGMENT;

  // map S-mode trap vector section in user space (direct mapping as in kernel space)
  // we assume that the size of usertrap.S is smaller than a page.
  user_vm_map((pagetable_t)procs[i].pagetable, (uint64)trap_sec_start, PGSIZE,
              (uint64)trap_sec_start, prot_to_type(PROT_READ | PROT_EXEC, 0));
  procs[i].mapped_info[SYSTEM_SEGMENT].va = (uint64)trap_sec_start;
  procs[i].mapped_info[SYSTEM_SEGMENT].npages = 1;
  procs[i].mapped_info[SYSTEM_SEGMENT].seg_type = SYSTEM_SEGMENT;

  sprint("in alloc_proc. user frame 0x%lx, user stack 0x%lx, user kstack 0x%lx \n",
         procs[i].trapframe, procs[i].trapframe->regs.sp, procs[i].kstack);

  // initialize the process's heap manager
  procs[i].user_heap.heap_top = USER_FREE_ADDRESS_START;
  procs[i].user_heap.heap_bottom = USER_FREE_ADDRESS_START;
  procs[i].user_heap.free_pages_count = 0;

  // map user heap in userspace
  procs[i].mapped_info[HEAP_SEGMENT].va = USER_FREE_ADDRESS_START;
  procs[i].mapped_info[HEAP_SEGMENT].npages = 0; // no pages are mapped to heap yet.
  procs[i].mapped_info[HEAP_SEGMENT].seg_type = HEAP_SEGMENT;

  procs[i].total_mapped_region = 4;

  // return after initialization.
  return &procs[i];
}

//
// reclaim a process. added @lab3_1
//
int free_process(process *proc)
{
  // we set the status to ZOMBIE, but cannot destruct its vm space immediately.
  // since proc can be current process, and its user kernel stack is currently in use!
  // but for proxy kernel, it (memory leaking) may NOT be a really serious issue,
  // as it is different from regular OS, which needs to run 7x24.
  proc->status = ZOMBIE;
  // sprint("process %d is free.\n", proc->pid);
  if (proc->pid == 0) // 0号进程退出，程序终止
    return 0;
  return 0;
}

int do_fork(process *parent)
{
  sprint("will fork a child from parent %d.\n", parent->pid);
  process *child = alloc_process();
  for (int i = 0; i < parent->total_mapped_region; i++)
  {
    switch (parent->mapped_info[i].seg_type)
    {
    case CONTEXT_SEGMENT:
      *child->trapframe = *parent->trapframe;
      break;
    case STACK_SEGMENT:
      memcpy((void *)lookup_pa(child->pagetable, child->mapped_info[0].va),
             (void *)lookup_pa(parent->pagetable, parent->mapped_info[i].va), PGSIZE);
      break;
    case CODE_SEGMENT:
      for (int j = 0; j < parent->mapped_info[i].npages; j++)
      {
        uint64 addr = lookup_pa(parent->pagetable, parent->mapped_info[i].va + j * PGSIZE);
        // 使用写时复制模式映射代码段
        map_pages(child->pagetable, parent->mapped_info[i].va + j * PGSIZE, PGSIZE,
                  addr, prot_to_type(PROT_READ | PROT_EXEC, 1)); // 只读映射
        sprint("do_fork map code segment at pa:%lx of parent to child at va:%lx.\n",
               addr, parent->mapped_info[i].va + j * PGSIZE);
      }
      child->mapped_info[child->total_mapped_region].va = parent->mapped_info[i].va;
      child->mapped_info[child->total_mapped_region].npages =
          parent->mapped_info[i].npages;
      child->mapped_info[child->total_mapped_region].seg_type = CODE_SEGMENT;
      child->total_mapped_region++;
      break;
    case DATA_SEGMENT:
      for (int j = 0; j < parent->mapped_info[i].npages; j++)
      {
        uint64 addr = lookup_pa(parent->pagetable, parent->mapped_info[i].va + j * PGSIZE);
        // 使用写时复制模式映射数据段
        map_pages(child->pagetable, parent->mapped_info[i].va + j * PGSIZE, PGSIZE,
                  addr, prot_to_type(PROT_READ, 1)); // 只读映射
      }
      child->mapped_info[child->total_mapped_region].va = parent->mapped_info[i].va;
      child->mapped_info[child->total_mapped_region].npages =
          parent->mapped_info[i].npages;
      child->mapped_info[child->total_mapped_region].seg_type = DATA_SEGMENT;
      child->total_mapped_region++;
      break;
    // added on lab3_c3
    case HEAP_SEGMENT:
      // 需要复制父进程的堆空间
      for (uint64 heap_block = current->user_heap.heap_bottom;
          heap_block < current->user_heap.heap_top; heap_block += PGSIZE) 
      {
        uint64 parent_pa = lookup_pa(parent->pagetable, heap_block);
        // 使用写时复制模式映射堆空间
        user_vm_map((pagetable_t)child->pagetable, heap_block, PGSIZE, parent_pa,
                    prot_to_type(PROT_READ, 1));
        // 设置pte
        pte_t *child_pte = page_walk(child->pagetable, heap_block, 0);
        if(child_pte == NULL) {
          panic("error when mapping heap segment!");
        }
        *child_pte |= PTE_C; // 设置写时复制标志
        pte_t *parent_pte = page_walk(parent->pagetable, heap_block, 0);
        if(parent_pte == NULL) {
          panic("error when mapping heap segment!");
        }
        *parent_pte &= (~PTE_W); // 设置父进程的pte为只读

      }
      child->mapped_info[HEAP_SEGMENT].npages = parent->mapped_info[HEAP_SEGMENT].npages;
      memcpy((void*)&child->user_heap, (void*)&parent->user_heap, sizeof(parent->user_heap));
      break;
    }
  }
  child->status = READY;
  child->trapframe->regs.a0 = 0;
  child->parent = parent;
  insert_to_ready_queue(child);
  return child->pid;
}
