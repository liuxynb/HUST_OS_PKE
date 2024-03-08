/*
 * implementing the scheduler
 */

#include "sched.h"
#include "spike_interface/spike_utils.h"

//
// insert a process, proc, into the END of ready queue.
//

process *ready_queue_head = NULL;

void insert_to_ready_queue(process *proc)
{
  sprint("going to insert process %d to ready queue.\n", proc->pid);
  // if the queue is empty in the beginning
  if (ready_queue_head == NULL)
  {
    proc->status = READY;
    proc->queue_next = NULL;
    ready_queue_head = proc;
    return;
  }

  // ready queue is not empty
  process *p;
  // browse the ready queue to see if proc is already in-queue
  for (p = ready_queue_head; p->queue_next != NULL; p = p->queue_next)
    if (p == proc)
      return; // already in queue

  // p points to the last element of the ready queue
  if (p == proc)
    return;
  p->queue_next = proc;
  proc->status = READY;
  proc->queue_next = NULL;

  return;
}

process *blocked_queue_head = NULL;

//
// insert a process, proc, into the END of blocked queue. added on lab3_challenge
//
void insert_to_blocked_queue(process *proc)
{
  // sprint("going to insert process %d to blocked queue.\n", proc->pid);
  // move the proc out of ready queue if it is in ready queue
  // sprint("Now checking if process %d is in ready queue.\n", proc->pid);
  if (ready_queue_head != NULL)
  {
    // sprint("ready queue is not empty.\n");
    // sprint("checking process %d.\n", ready_queue_head->pid);
    if (ready_queue_head == proc)
    {
      // sprint("process %d is in ready queue, now moving it out.\n", proc->pid);
      ready_queue_head = ready_queue_head->queue_next;
      return;
    }
    else
    {
      process *p;
      for (p = ready_queue_head; p->queue_next != NULL; p = p->queue_next)
      {
        // sprint("checking process %d.\n", p->queue_next->pid);
        if (p->queue_next == proc)
        {
          // sprint("process %d is in ready queue, now moving it out.\n", p->queue_next->pid);
          p->queue_next = p->queue_next->queue_next;
          return;
        }
      }
    }
    // sprint("process %d is not in ready queue.\n", proc->pid);
  }

  // if the queue is empty in the beginning
  if (blocked_queue_head == NULL)
  {
    proc->status = BLOCKED;
    proc->queue_next = NULL;
    blocked_queue_head = proc;
    // sprint("blocked queue is empty, process %d is blocked.\n", proc->pid);
    return;
  }
  // blocked queue is not empty
  process *p;
  // browse the blocked queue to see if proc is already in-queue
  for (p = blocked_queue_head; p->queue_next != NULL; p = p->queue_next)
    if (p == proc)
    {
      // sprint("process %d is already in blocked queue.\n", proc->pid);
      return; // already in queue
    }

  // p points to the last element of the blocked queue
  if (p == proc)
    return;
  p->queue_next = proc;
  proc->status = BLOCKED;
  proc->queue_next = NULL;
  // sprint("process %d is blocked.\n", proc->pid);
  return;
}

//
// choose a proc from the ready queue, and put it to run.
// note: schedule() does not take care of previous current process. If the current
// process is still runnable, you should place it into the ready queue (by calling
// ready_queue_insert), and then call schedule().
//
extern process procs[NPROC];
void schedule()
{
  // sprint("going to schedule process to run.\n");
  if (!ready_queue_head)
  {
    // by default, if there are no ready process, and all processes are in the status of
    // FREE and ZOMBIE, we should shutdown the emulated RISC-V machine.
    int should_shutdown = 1;

    for (int i = 0; i < NPROC; i++)
      if ((procs[i].status != FREE) && (procs[i].status != ZOMBIE))
      {
        should_shutdown = 0;
        sprint("ready queue empty, but process %d is not in free/zombie state:%d\n",
               i, procs[i].status);
      }

    if (should_shutdown)
    {
      sprint("no more ready processes, system shutdown now.\n");
      shutdown(0);
    }
    else
    {
      panic("Not handled: we should let system wait for unfinished processes.\n");
    }
  }

  current = ready_queue_head;
  assert(current->status == READY);
  ready_queue_head = ready_queue_head->queue_next;

  current->status = RUNNING;
  sprint("going to schedule process %d to run.\n", current->pid);
  switch_to(current);
}

//
// move the blocked process to ready queue if it is blocked by its parent
//
void wake_up(process *proc)
{
  // sprint("the parent process now on wake_up is %d, its parent is %d\n", proc->pid, proc->parent->pid);
  // added on @lab3_challenge1 for blocked process to wake up.
  if (blocked_queue_head != NULL)
  {
    // sprint("blocked queue is not empty.\n");
    process *p = blocked_queue_head;
    // sprint("checking process %d.\n", p->pid);
    if (p->pid == proc->parent->pid)
    {
      blocked_queue_head = blocked_queue_head->queue_next;
      // sprint("process %d is waken up.\n", p->pid);
      insert_to_ready_queue(p);
    }
    else
    {
      for (; p->queue_next != NULL; p = p->queue_next)
      {
        // sprint("checking process %d.\n", p->queue_next->pid);
        if (p->queue_next->pid == proc->parent->pid)
        {
          // sprint("process %d is waken up.\n", p->queue_next->pid);
          insert_to_ready_queue(p->queue_next);
          p->queue_next = p->queue_next->queue_next;
          break;
        }
      }
    }
  }
  else
  {
    // sprint("blocked queue is empty.\n");
  }
}