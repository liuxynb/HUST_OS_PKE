/*
 * implementing the scheduler
 */

#include "sched.h"
#include "spike_interface/spike_utils.h"
#include "vmm.h"
#include "sync_utils.h"

process* ready_queue_head[NCPU] = {NULL};
process* blocked_queue_head[NCPU] = {NULL};
//
// insert a process, proc, into the END of ready queue.
//
void insert_to_ready_queue( process* proc ) {
  
  int hartid = read_tp();
  // sprint( "hartid = %d : going to insert process %d to ready queue.\n",hartid,proc->pid );
  // if the queue is empty in the beginning
  if( ready_queue_head[hartid] == NULL ){
    proc->status = READY;
    proc->queue_next = NULL;
    ready_queue_head[hartid] = proc;
    return;
  }

  // ready queue is not empty
  process *p;
  // browse the ready queue to see if proc is already in-queue
  for( p=ready_queue_head[hartid]; p->queue_next!=NULL; p=p->queue_next )
    if( p == proc ) return;  //already in queue

  // p points to the last element of the ready queue
  if( p==proc ) return;
  p->queue_next = proc;
  proc->status = READY;
  proc->queue_next = NULL;

  return;
}

void insert_to_blocked_queue( process* proc ){
  int hartid = read_tp();
  // sprint( "hartid %d : going to insert process %d to blocked queue.\n", hartid,proc->pid );
  if( blocked_queue_head[read_tp()] == NULL ){
    proc->status = BLOCKED;
    proc->queue_next = NULL;
    blocked_queue_head[read_tp()] = proc;
    return;
  }
  process *p;
  for( p=blocked_queue_head[read_tp()]; p->queue_next!=NULL; p=p->queue_next )
    if( p == proc ) return;  //already in queue


  if( p==proc ) return;
  p->queue_next = proc;
  proc->status = BLOCKED;
  proc->queue_next = NULL;
  return;
}

//
// choose a proc from the ready queue, and put it to run.
// note: schedule() does not take care of previous current[hartid] process. If the current[hartid]
// process is still runnable, you should place it into the ready queue (by calling
// ready_queue_insert), and then call schedule().
//
extern process procs[NPROC];
static int exit_cnt = 0;
void schedule() {
  int hartid = read_tp();
  if ( !ready_queue_head[hartid] ){
    // by default, if there are no ready process, and all processes are in the status of
    // FREE and ZOMBIE, we should shutdown the emulated RISC-V machine.

    int should_shutdown = 1;

    for( int i=0; i<NPROC; i++ )
      if( (procs[i].status != FREE) && (procs[i].status != ZOMBIE) && procs[i].hartid == hartid){
        should_shutdown = 0;
        // sprint( "ready queue empty, but process %d is not in free/zombie state:%d\n", 
          // i, procs[i].status );
      }

    if( should_shutdown ){
      if(hartid ==0){
      shutdown( 0 );
      } 
    }else{
      panic( "Not handled: we should let system wait for unfinished processes.\n" );
    }
  }

  current[hartid] = ready_queue_head[hartid];
  assert( current[hartid]->status == READY );
  ready_queue_head[hartid] = ready_queue_head[hartid]->queue_next;

  current[hartid]->status = RUNNING;
  // sprint( "hartid = %d : going to schedule process %d to run.\n",hartid,current[hartid]->pid );
  switch_to( current[hartid] );
}

void awake_father_process( process* child ){
    /**
     * 输入：child--结束的子进程
     * 功能：唤醒child的被阻塞的父进程
     */
    // sprint("awake_father_process\n");
    if(blocked_queue_head[read_tp()]==NULL) return;//没有被阻塞的进程，直接返回
    // else sprint("blocked_queue_head[read_tp()] is not NULL\n");
    process * waked_proc=NULL;//被唤醒的父进程
    process * p=blocked_queue_head[read_tp()],*q=NULL;
    for(;p!=NULL;q=p,p=p->queue_next){//遍历阻塞队列
        // sprint("pid %d\n",p->pid);
        if(p==child->parent){//找到了child的父进程，将其从阻塞队列中移除，加入就绪队列
            waked_proc=p;
            if(q==NULL){//如果是队首
                blocked_queue_head[read_tp()]=p->queue_next;
            }else{
                q->queue_next=p->queue_next;
            }
            waked_proc->status=READY;
            insert_to_ready_queue(waked_proc);
            return;//找到了父进程，直接返回
        }
    }
    
}