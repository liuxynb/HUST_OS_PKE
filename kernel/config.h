#ifndef _CONFIG_H_
#define _CONFIG_H_

// we use two HART (cpu) in challenge3
#define NCPU 2

//interval of timer interrupt. added @lab1_3
#define TIMER_INTERVAL 1000000

#define DRAM_BASE 0x80000000

#define STACK_SIZE 0x5000000

/* we use fixed physical (also logical) addresses for the stacks and trap frames as in
 Bare memory-mapping mode */
// user stack top
#define USER_STACK(i) (0x81100000 + i * STACK_SIZE)

// the stack used by PKE kernel when a syscall happens
#define USER_KSTACK(i) (0x81200000 + i * STACK_SIZE) 

// the trap frame used to assemble the user "process"
#define USER_TRAP_FRAME(i) (0x81300000 + i * STACK_SIZE)

#endif
