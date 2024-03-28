/*
 * Utility functions for process management.
 *
 * Note: in Lab1, only one process (i.e., our user application) exists. Therefore,
 * PKE OS at this stage will set "current[hartid]" to the loaded user application, and also
 * switch to the old "current[hartid]" process after trap handling.
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
#include "sync_utils.h"

// Two functions defined in kernel/usertrap.S
extern char smode_trap_vector[];

extern void return_to_user(trapframe *, uint64 satp);

// trap_sec_start points to the beginning of S-mode trap segment (i.e., the entry point
// of S-mode trap vector).
extern char trap_sec_start[];

// process pool. added @lab3_1
process procs[NPROC];
semaphore sem_pool[NPROC];

// current[hartid] points to the current[hartid]ly running user-mode application.
process *current[NCPU] = {NULL};

// points to the first free page in our simple heap. added @lab2_2
uint64 g_ufree_page[NCPU] = {USER_FREE_ADDRESS_START};

//
// switch to a user-mode process
//
void switch_to(process *proc)
{
    // sprint("switch_to process %d.\n", proc->pid);
    int hartid = read_tp();
    assert(proc);
    current[hartid] = proc;
    proc->hartid = hartid;
    // current[proc->hartid] = proc;
    // sprint("hartid = %d,proc->hartid = %d\n",hartid,proc->hartid);

    // write the smode_trap_vector (64-bit func. address) defined in kernel/strap_vector.S
    // to the stvec privilege register, such that trap handler pointed by smode_trap_vector
    // will be triggered when an interrupt occurs in S mode.
    write_csr(stvec, (uint64)smode_trap_vector);

    // set up trapframe values (in process structure) that smode_trap_vector will need when
    // the process next re-enters the kernel.
    proc->trapframe->kernel_sp = proc->kstack;     // process's kernel stack
    proc->trapframe->kernel_satp = read_csr(satp); // kernel page table
    proc->trapframe->kernel_trap = (uint64)smode_trap_handler;
    proc->trapframe->regs.tp = hartid; // process's hart id

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
    // sprint("process %d is going to be switched to user mode.\n", proc->pid);
    // sprint("user_satp: 0x%lx\n", user_satp);
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

    procs[i].kstack = (uint64)
                          alloc_page() +
                      PGSIZE; // user kernel stack top
    uint64
        user_stack = (uint64)
            alloc_page();                         // phisical address of user stack bottom
    procs[i].trapframe->regs.sp = USER_STACK_TOP; // virtual address of user stack top

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
                (uint64)
                    procs[i]
                        .trapframe,
                prot_to_type(PROT_WRITE | PROT_READ, 0));
    procs[i].mapped_info[CONTEXT_SEGMENT].va = (uint64)
                                                   procs[i]
                                                       .trapframe;
    procs[i].mapped_info[CONTEXT_SEGMENT].npages = 1;
    procs[i].mapped_info[CONTEXT_SEGMENT].seg_type = CONTEXT_SEGMENT;

    // map S-mode trap vector section in user space (direct mapping as in kernel space)
    // we assume that the size of usertrap.S is smaller than a page.
    user_vm_map((pagetable_t)procs[i].pagetable, (uint64)trap_sec_start, PGSIZE,
                (uint64)
                    trap_sec_start,
                prot_to_type(PROT_READ | PROT_EXEC, 0));
    procs[i].mapped_info[SYSTEM_SEGMENT].va = (uint64)
        trap_sec_start;
    procs[i].mapped_info[SYSTEM_SEGMENT].npages = 1;
    procs[i].mapped_info[SYSTEM_SEGMENT].seg_type = SYSTEM_SEGMENT;

    // sprint("in alloc_proc. user frame 0x%lx, user stack 0x%lx, user kstack 0x%lx \n",
    //    procs[i].trapframe, procs[i].trapframe->regs.sp, procs[i].kstack);

    // initialize the process's heap manager
    procs[i].user_heap.heap_top = USER_FREE_ADDRESS_START;
    procs[i].user_heap.heap_bottom = USER_FREE_ADDRESS_START;
    procs[i].user_heap.free_pages_count = 0;

    // map user heap in userspace
    procs[i].mapped_info[HEAP_SEGMENT].va = USER_FREE_ADDRESS_START;
    procs[i].mapped_info[HEAP_SEGMENT].npages = 0; // no pages are mapped to heap yet.
    procs[i].mapped_info[HEAP_SEGMENT].seg_type = HEAP_SEGMENT;

    procs[i].total_mapped_region = 4;
    procs[i].hartid = read_tp();
    // initialize files_struct
    procs[i].pfiles = init_proc_file_management();
    return &procs[i];
}

//
// reclaim a process. added @lab3_1
//
int free_process(process *proc)
{
    // we set the status to ZOMBIE, but cannot destruct its vm space immediately.
    // since proc can be current[hartid] process, and its user kernel stack is current[hartid]ly in use!
    // but for proxy kernel, it (memory leaking) may NOT be a really serious issue,
    // as it is different from regular OS, which needs to run 7x24.
    proc->status = ZOMBIE;
    if (proc->pid == 0)
    {
        // sprint("process 0 is going to be reclaimed\n");
        return 0;
    }
    awake_father_process(proc);
    return 0;
}

//
// implements fork syscal in kernel. added @lab3_1
// basic idea here is to first allocate an empty process (child), then duplicate the
// context and data segments of parent process to the child, and lastly, map other
// segments (code, system) of the parent to child. the stack segment remains unchanged
// for the child.
//
int do_fork(process *parent)
{
    // sprint("will fork a child from parent %d.\n", parent->pid);
    process *child = alloc_process();
    int hartid = read_tp();
    // sprint("do_fork\n");
    for (int i = 0; i < parent->total_mapped_region; i++)
    {
        // browse parent's vm space, and copy its trapframe and data segments,
        // map its code segment.

        switch (parent->mapped_info[i].seg_type)
        {
        case CONTEXT_SEGMENT:
        {
            *child->trapframe = *parent->trapframe;
            break;
        }
        case STACK_SEGMENT:
            memcpy((void *)lookup_pa(child->pagetable, child->mapped_info[STACK_SEGMENT].va),
                   (void *)lookup_pa(parent->pagetable, parent->mapped_info[i].va), PGSIZE);
            break;
        case HEAP_SEGMENT:
            // build a same heap for child process.

            // convert free_pages_address into a filter to skip reclaimed blocks in the heap
            // when mapping the heap blocks
            {
                int free_block_filter[MAX_HEAP_PAGES];
                memset(free_block_filter, 0, MAX_HEAP_PAGES);
                uint64 heap_bottom = parent->user_heap.heap_bottom;
                for (int i = 0; i < parent->user_heap.free_pages_count; i++)
                {
                    int index = (parent->user_heap.free_pages_address[i] - heap_bottom) / PGSIZE;
                    free_block_filter[index] = 1;
                }

                // copy and map the heap blocks
                for (uint64 heap_block = current[hartid]->user_heap.heap_bottom;
                     heap_block < current[hartid]->user_heap.heap_top; heap_block += PGSIZE)
                {
                    if (free_block_filter[(heap_block - heap_bottom) / PGSIZE]) // skip free blocks
                        continue;

                    // lab3_ch3 cow 只映射不拷贝
                    uint64 pa = lookup_pa(parent->pagetable, heap_block);
                    user_vm_map((pagetable_t)child->pagetable, heap_block, PGSIZE, pa, prot_to_type(PROT_READ, 1));
                    pte_t *pte = page_walk((pagetable_t)child->pagetable, heap_block, 0);
                    if (pte == NULL)
                    {
                        panic("segment mapping fault\n");
                    }
                    *pte |= PTE_C;
                    pte = page_walk((pagetable_t)parent->pagetable, heap_block, 0);
                    if (pte == NULL)
                    {
                        panic("parent page faults\n");
                    }
                    // 父进程的堆设置为只读
                    *pte &= ~PTE_W;
                }

                child->mapped_info[HEAP_SEGMENT].npages = parent->mapped_info[HEAP_SEGMENT].npages;

                // copy the heap manager from parent to child
                memcpy((void *)&child->user_heap, (void *)&parent->user_heap, sizeof(parent->user_heap));
                break;
            }
        case CODE_SEGMENT:
        {
            uint64 child_va = parent->mapped_info[i].va;
            uint64 child_pa = lookup_pa(parent->pagetable, child_va);
            user_vm_map((pagetable_t)child->pagetable, child_va, PGSIZE, child_pa,
                        prot_to_type(PROT_READ | PROT_EXEC, 1));
            // after mapping, register the vm region (do not delete codes below!)
            child->mapped_info[child->total_mapped_region].va = parent->mapped_info[i].va;
            child->mapped_info[child->total_mapped_region].npages =
                parent->mapped_info[i].npages;
            child->mapped_info[child->total_mapped_region].seg_type = CODE_SEGMENT;
            child->total_mapped_region++;
            break;
        }
        // copy the data segment from parent to child
        case DATA_SEGMENT:
        {
            for (int j = 0; j < parent->mapped_info[i].npages; j++)
            {
                uint64 addr = lookup_pa(parent->pagetable, parent->mapped_info[i].va + j * PGSIZE);
                char *newaddr = alloc_page();
                memcpy(newaddr, (void *)addr, PGSIZE);
                map_pages(child->pagetable, parent->mapped_info[i].va + j * PGSIZE, PGSIZE,
                          (uint64)newaddr, prot_to_type(PROT_WRITE | PROT_READ, 1));
            }
            // after mapping, register the vm region (do not delete codes below!)
            child->mapped_info[child->total_mapped_region].va = parent->mapped_info[i].va;
            child->mapped_info[child->total_mapped_region].npages =
                parent->mapped_info[i].npages;
            child->mapped_info[child->total_mapped_region].seg_type = DATA_SEGMENT;
            child->total_mapped_region++;
            break;
        }
        }
    }

    child->status = READY;
    child->trapframe->regs.a0 = 0;
    child->parent = parent;
    child->pfiles = parent->pfiles;
    child->n_stack_pages = 1;
    insert_to_ready_queue(child);
    return child->pid;
}

int do_wait(int pid)
{
    sprint("pid = %d : do_wait\n", current[read_tp()]->pid);
    int flag = 0; // 标志是否找到子进程
    int hartid = read_tp();
    for (int i = 0; i < NPROC; i++)
    {
        if (procs[i].parent == current[hartid] && procs[i].status == ZOMBIE)
        { // 找到了一个僵尸进程
            // sprint("find a zombie process %d\n",procs[i].pid);
            procs[i].status = FREE;
        }
    }
    if (pid == -1)
    { // 父进程任意等待一个子进程
        for (int i = 0; i < NPROC; i++)
        {
            if (procs[i].parent == current[hartid])
            {
                flag = 1;
                if (procs[i].status == ZOMBIE)
                { // 找到了一个僵尸进程
                    procs[i].status = FREE;
                    return procs[i].pid; // 返回pid还是i都可以，初始化都一样
                }
            }
        }
        if (flag == 0)
        {
            return -1; // 没有找到子进程
        }
        else
        { // 子进程还没有执行完，进入阻塞队列
            current[hartid]->status = BLOCKED;
            insert_to_blocked_queue(current[hartid]);
            schedule();
            return -2;
        }
    }
    else if (pid >= 0 && pid < NPROC)
    { // 等待特定的子进程
        if (procs[pid].parent != current[hartid])
            return -1;
        else
        {
            if (procs[pid].status == ZOMBIE)
            { // 找到了一个僵尸进程
                procs[pid].status = FREE;
                return procs[pid].pid; // 返回pid还是i都可以，初始化都一样
            }
            else
            {
                current[hartid]->status = BLOCKED;
                insert_to_blocked_queue(current[hartid]);
                schedule();
                return -2;
            }
        }
    }
    else
    { // pid 非法
        return -1;
    }
    return 0;
}

int do_sem_new(int value)
{
    for (int i = 0; i < NPROC; i++)
    {
        if (sem_pool[i].is_occupied == 0)
        {
            // 找到了一个空闲的信号量,进行初始化
            sem_pool[i].is_occupied = 1;
            sem_pool[i].value = value;
            sem_pool[i].waiting_queue_head = NULL;
            sem_pool[i].waiting_queue_tail = NULL;
            return i;
        }
    }
    panic("No available semaphore");
    return -1;
}
int do_sem_p(int sem_id)
{
    int hartid = read_tp();
    if (sem_id < 0 || sem_id >= NPROC)
    {
        panic("sem_id is illegal");
        return -1;
    }

    sem_pool[sem_id].value--;
    if (sem_pool[sem_id].value < 0)
    {
        // 将当前进程加入等待队列
        if (sem_pool[sem_id].waiting_queue_head == NULL)
        {
            sem_pool[sem_id].waiting_queue_head = current[hartid];
            sem_pool[sem_id].waiting_queue_tail = current[hartid];
            current[hartid]->queue_next = NULL;
        }
        else
        {
            sem_pool[sem_id].waiting_queue_tail->queue_next = current[hartid];
            sem_pool[sem_id].waiting_queue_tail = current[hartid];
            current[hartid]->queue_next = NULL;
        }
        current[hartid]->status = BLOCKED; // 将当前进程状态设置为阻塞
        schedule();                        // 调度下一个进程
        return 0;
    }
    return 0;
}
int do_sem_v(int sem_id)
{
    if (sem_id < 0 || sem_id >= NPROC)
    {
        panic("sem_id is illegal");
        return -1;
    }
    sem_pool[sem_id].value++;
    if (sem_pool[sem_id].waiting_queue_head != NULL)
    {
        // 采用FIFO的方式唤醒等待队列中的进程
        process *p = sem_pool[sem_id].waiting_queue_head;
        if (sem_pool[sem_id].waiting_queue_head == sem_pool[sem_id].waiting_queue_tail)
        {
            sem_pool[sem_id].waiting_queue_head = NULL;
            sem_pool[sem_id].waiting_queue_tail = NULL;
        }
        else
        {
            sem_pool[sem_id].waiting_queue_head = p->queue_next;
        }
        p->queue_next = NULL;
        p->status = READY;
        insert_to_ready_queue(p);
    }
    return 0;
}
int do_sem_free(int sem_id)
{
    if (sem_id < 0 || sem_id >= NPROC)
    {
        panic("sem_id is illegal");
        return -1;
    }
    if (sem_pool[sem_id].waiting_queue_head != NULL)
    {
        panic("There are still processes waiting for this semaphore");
        return -1;
    }
    sem_pool[sem_id].is_occupied = 0;
    return 0;
}

void init_process(process *p)
{

    p->trapframe = (trapframe *)alloc_page(); // trapframe, used to save context
    memset(p->trapframe, 0, sizeof(trapframe));

    p->pagetable = (pagetable_t)alloc_page();
    memset((void *)p->pagetable, 0, PGSIZE);

    p->kstack = (uint64)alloc_page() + PGSIZE; // user kernel stack top
    uint64 user_stack = (uint64)alloc_page();  // phisical address of user stack bottom
    p->trapframe->regs.sp = USER_STACK_TOP;    // virtual address of user stack top

    p->mapped_info = (mapped_region *)alloc_page(); // mapped_info
    memset(p->mapped_info, 0, PGSIZE);

    user_vm_map((pagetable_t)p->pagetable, USER_STACK_TOP - PGSIZE, PGSIZE, // map user stack in userspace
                user_stack, prot_to_type(PROT_WRITE | PROT_READ, 1));
    p->mapped_info[STACK_SEGMENT].va = USER_STACK_TOP - PGSIZE;
    p->mapped_info[STACK_SEGMENT].npages = 1;
    p->mapped_info[STACK_SEGMENT].seg_type = STACK_SEGMENT;

    // map trapframe in user space (direct mapping as in kernel space).
    user_vm_map((pagetable_t)p->pagetable, (uint64)p->trapframe, PGSIZE,
                (uint64)p->trapframe, prot_to_type(PROT_WRITE | PROT_READ, 0));
    p->mapped_info[CONTEXT_SEGMENT].va = (uint64)p->trapframe;
    p->mapped_info[CONTEXT_SEGMENT].npages = 1;
    p->mapped_info[CONTEXT_SEGMENT].seg_type = CONTEXT_SEGMENT;

    user_vm_map((pagetable_t)p->pagetable, (uint64)trap_sec_start, PGSIZE,
                (uint64)trap_sec_start, prot_to_type(PROT_READ | PROT_EXEC, 0));
    p->mapped_info[SYSTEM_SEGMENT].va = (uint64)trap_sec_start;
    p->mapped_info[SYSTEM_SEGMENT].npages = 1;
    p->mapped_info[SYSTEM_SEGMENT].seg_type = SYSTEM_SEGMENT;

    // heap management
    p->user_heap.heap_top = USER_FREE_ADDRESS_START;
    p->user_heap.heap_bottom = USER_FREE_ADDRESS_START;
    p->user_heap.free_pages_count = 0;

    p->mapped_info[HEAP_SEGMENT].va = USER_FREE_ADDRESS_START;
    p->mapped_info[HEAP_SEGMENT].npages = 0; // no pages are mapped to heap yet.
    p->mapped_info[HEAP_SEGMENT].seg_type = HEAP_SEGMENT;

    p->total_mapped_region = 4;

    // file management
    p->pfiles = init_proc_file_management();
}

// added @lab4_c2
// reclaim the open-file management data structure of a process.
// exec会根据读入的可执行文件将'原进程'的数据段、代码段和堆栈段替换。
int do_exec(char *path, char *para)
{
    int hartid = read_tp();
    init_process(current[hartid]);
    if(load_bincode_from_host_elf(current[hartid], path) == -1)
    {
        sprint("path error!\n");
        return -1;
    }
    // push into stack
    size_t *vsp = (size_t *)current[hartid]->trapframe->regs.sp;
    vsp -= 8;
    size_t *sp = (size_t *)user_va_to_pa(current[hartid]->pagetable, (void *)vsp);
    memcpy((char *)sp, para, 1 + strlen(para));
    vsp--, sp--;
    *sp = (size_t)(1 + vsp);

    // reg
    current[hartid]->trapframe->regs.sp = (uint64)vsp;
    current[hartid]->trapframe->regs.a0 = (uint64)1;
    current[hartid]->trapframe->regs.a1 = (uint64)vsp;
    return 0;
}
void copy_on_write_on_heap(process *child, process *parent, uint64 pa)
{
    // sprint("copy on write\n");
    for (uint64 heap_block = parent->user_heap.heap_bottom;
         heap_block < parent->user_heap.heap_top; heap_block += PGSIZE)
    {
        uint64 heap_block_pa = lookup_pa(parent->pagetable, heap_block);
        if (heap_block_pa == 0)
            panic("error on looking up heap_block pa!");
        if (heap_block_pa >= pa && heap_block_pa < pa + PGSIZE)
        {
            user_vm_unmap(child->pagetable, heap_block, PGSIZE, 0); // 取消映射
            void *pa = alloc_page();
            if ((void *)pa == NULL)
                panic("Can not allocate a new physical page.\n");
            pte_t *child_pte = page_walk(child->pagetable, heap_block, 0);
            if (child_pte == NULL)
            {
                panic("error when mapping heap segment!");
            }
            *child_pte |= (~PTE_C);      // 设置写时复制标志，为已复制
            *child_pte &= PTE_W | PTE_R; // 设置读写权限
            user_vm_map(child->pagetable, heap_block, PGSIZE, (uint64)pa, prot_to_type(PROT_WRITE | PROT_READ, 1));
            memcpy(pa, (void *)lookup_pa(parent->pagetable, heap_block), PGSIZE);
            break;
        }
    }
}