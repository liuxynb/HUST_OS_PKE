/*
 * virtual address mapping related functions.
 */

#include "vmm.h"
#include "riscv.h"
#include "pmm.h"
#include "util/types.h"
#include "memlayout.h"
#include "process.h"
#include "syscall.h"
#include "util/string.h"
#include "spike_interface/spike_utils.h"
#include "util/functions.h"


int init_flag = 0;//mem_control_block链表是否初始化的标志

/* --- utility functions for virtual address mapping --- */
//
// establish mapping of virtual address [va, va+size] to phyiscal address [pa, pa+size]
// with the permission of "perm".
//
int map_pages(pagetable_t page_dir, uint64 va, uint64 size, uint64 pa, int perm) {
  uint64 first, last;
  pte_t *pte;

  for (first = ROUNDDOWN(va, PGSIZE), last = ROUNDDOWN(va + size - 1, PGSIZE);
      first <= last; first += PGSIZE, pa += PGSIZE) {
    if ((pte = page_walk(page_dir, first, 1)) == 0) return -1;
    if (*pte & PTE_V)
      panic("map_pages fails on mapping va (0x%lx) to pa (0x%lx)", first, pa);
    *pte = PA2PTE(pa) | perm | PTE_V;
  }
  return 0;
}

//
// convert permission code to permission types of PTE
//
uint64 prot_to_type(int prot, int user) {
  uint64 perm = 0;
  if (prot & PROT_READ) perm |= PTE_R | PTE_A;
  if (prot & PROT_WRITE) perm |= PTE_W | PTE_D;
  if (prot & PROT_EXEC) perm |= PTE_X | PTE_A;
  if (perm == 0) perm = PTE_R;
  if (user) perm |= PTE_U;
  return perm;
}

//
// traverse the page table (starting from page_dir) to find the corresponding pte of va.
// returns: PTE (page table entry) pointing to va.
//
pte_t *page_walk(pagetable_t page_dir, uint64 va, int alloc) {
  if (va >= MAXVA) panic("page_walk");

  // starting from the page directory
  pagetable_t pt = page_dir;

  // traverse from page directory to page table.
  // as we use risc-v sv39 paging scheme, there will be 3 layers: page dir,
  // page medium dir, and page table.
  for (int level = 2; level > 0; level--) {
    // macro "PX" gets the PTE index in page table of current level
    // "pte" points to the entry of current level
    pte_t *pte = pt + PX(level, va);

    // now, we need to know if above pte is valid (established mapping to a phyiscal page)
    // or not.
    if (*pte & PTE_V) {  //PTE valid
      // phisical address of pagetable of next level
      pt = (pagetable_t)PTE2PA(*pte);
    } else { //PTE invalid (not exist).
      // allocate a page (to be the new pagetable), if alloc == 1
      if( alloc && ((pt = (pte_t *)alloc_page(1)) != 0) ){
        memset(pt, 0, PGSIZE);
        // writes the physical address of newly allocated page to pte, to establish the
        // page table tree.
        *pte = PA2PTE(pt) | PTE_V;
      }else //returns NULL, if alloc == 0, or no more physical page remains
        return 0;
    }
  }

  // return a PTE which contains phisical address of a page
  return pt + PX(0, va);
}

//
// look up a virtual page address, return the physical page address or 0 if not mapped.
//
uint64 lookup_pa(pagetable_t pagetable, uint64 va) {
  pte_t *pte;
  uint64 pa;

  if (va >= MAXVA) return 0;

  pte = page_walk(pagetable, va, 0);
  if (pte == 0 || (*pte & PTE_V) == 0 || ((*pte & PTE_R) == 0 && (*pte & PTE_W) == 0))
    return 0;
  pa = PTE2PA(*pte);

  return pa;
}

/* --- kernel page table part --- */
// _etext is defined in kernel.lds, it points to the address after text and rodata segments.
extern char _etext[];

// pointer to kernel page director
pagetable_t g_kernel_pagetable;

//
// maps virtual address [va, va+sz] to [pa, pa+sz] (for kernel).
//
void kern_vm_map(pagetable_t page_dir, uint64 va, uint64 pa, uint64 sz, int perm) {
  // map_pages is defined in kernel/vmm.c
  if (map_pages(page_dir, va, sz, pa, perm) != 0) panic("kern_vm_map");
}

//
// kern_vm_init() constructs the kernel page table.
//
void kern_vm_init(void) {
  // pagetable_t is defined in kernel/riscv.h. it's actually uint64*
  pagetable_t t_page_dir;

  // allocate a page (t_page_dir) to be the page directory for kernel. alloc_page is defined in kernel/pmm.c
  t_page_dir = (pagetable_t)alloc_page();
  // memset is defined in util/string.c
  memset(t_page_dir, 0, PGSIZE);

  // map virtual address [KERN_BASE, _etext] to physical address [DRAM_BASE, DRAM_BASE+(_etext - KERN_BASE)],
  // to maintain (direct) text section kernel address mapping.
  kern_vm_map(t_page_dir, KERN_BASE, DRAM_BASE, (uint64)_etext - KERN_BASE,
         prot_to_type(PROT_READ | PROT_EXEC, 0));

  sprint("KERN_BASE 0x%lx\n", lookup_pa(t_page_dir, KERN_BASE));

  // also (direct) map remaining address space, to make them accessable from kernel.
  // this is important when kernel needs to access the memory content of user's app
  // without copying pages between kernel and user spaces.
  kern_vm_map(t_page_dir, (uint64)_etext, (uint64)_etext, PHYS_TOP - (uint64)_etext,
         prot_to_type(PROT_READ | PROT_WRITE, 0));

  sprint("physical address of _etext is: 0x%lx\n", lookup_pa(t_page_dir, (uint64)_etext));

  g_kernel_pagetable = t_page_dir;
}

/* --- user page table part --- */
//
// convert and return the corresponding physical address of a virtual address (va) of
// application.
//
void *user_va_to_pa(pagetable_t page_dir, void *va) {
  // TODO (lab2_1): implement user_va_to_pa to convert a given user virtual address "va"
  // to its corresponding physical address, i.e., "pa". To do it, we need to walk
  // through the page table, starting from its directory "page_dir", to locate the PTE
  // that maps "va". If found, returns the "pa" by using:
  // pa = PYHS_ADDR(PTE) + (va & (1<<PGSHIFT -1))
  // Here, PYHS_ADDR() means retrieving the starting address (4KB aligned), and
  // (va & (1<<PGSHIFT -1)) means computing the offset of "va" inside its page.
  // Also, it is possible that "va" is not mapped at all. in such case, we can find
  // invalid PTE, and should return NULL.
//  panic( "You have to implement user_va_to_pa (convert user va to pa) to print messages in lab2_1.\n" );

    uint64 pa = lookup_pa(page_dir,(uint64)va) +  ((uint64)(va) & ((1<<PGSHIFT)-1));//PPN + offset
    return (void *)pa;
}

//
// maps virtual address [va, va+sz] to [pa, pa+sz] (for user application).
//
void user_vm_map(pagetable_t page_dir, uint64 va, uint64 size, uint64 pa, int perm) {
  if (map_pages(page_dir, va, size, pa, perm) != 0) {
    panic("fail to user_vm_map .\n");
  }
}

//
// unmap virtual address [va, va+size] from the user app.
// reclaim the physical pages if free!=0
//
void user_vm_unmap(pagetable_t page_dir, uint64 va, uint64 size, int free) {
  // TODO (lab2_2): implement user_vm_unmap to disable the mapping of the virtual pages
  // in [va, va+size], and free the corresponding physical pages used by the virtual
  // addresses when if 'free' (the last parameter) is not zero.
  // basic idea here is to first locate the PTEs of the virtual pages, and then reclaim
  // (use free_page() defined in pmm.c) the physical pages. lastly, invalidate the PTEs.
  // as naive_free reclaims only one page at a time, you only need to consider one page
  // to make user/app_naive_malloc to behave correctly.
//  panic( "You have to implement user_vm_unmap to free pages using naive_free in lab2_2.\n" );
    // pte_t * pte = page_walk(page_dir,va,0);
    // if(pte == 0) return;//过滤掉无效的pte
    // void * pa = (void *)PTE2PA(*pte);  //获取pte对应的物理地址
    // *pte = *pte & (~PTE_V);  //将V位清零
    // free_page(pa);
    for(uint64 first = ROUNDDOWN(va, PGSIZE), last = ROUNDDOWN(va + size - 1, PGSIZE); first <= last; first += PGSIZE) {
    pagetable_t pt = page_dir;
    pte_t *pte;

    for(int level = 2; level >= 0; level--) {
      pte = pt + PX(level, first);
      if(((*pte) & PTE_V) == 0) {
        panic("unmap invalid addr!\n");
        return;
      }
      pt = (pagetable_t)PTE2PA(*pte);
    }
    *pte &= ~PTE_V;
    if(free) {
      free_page((void *)PTE2PA(*pte));
    }
  }
}

//
// debug function, print the vm space of a process. added @lab3_1
//
void print_proc_vmspace(process* proc) {
  sprint( "======\tbelow is the vm space of process%d\t========\n", proc->pid );
  for( int i=0; i<proc->total_mapped_region; i++ ){
    sprint( "-va:%lx, npage:%d, ", proc->mapped_info[i].va, proc->mapped_info[i].npages);
    switch(proc->mapped_info[i].seg_type){
      case CODE_SEGMENT: sprint( "type: CODE SEGMENT" ); break;
      case DATA_SEGMENT: sprint( "type: DATA SEGMENT" ); break;
      case STACK_SEGMENT: sprint( "type: STACK SEGMENT" ); break;
      case CONTEXT_SEGMENT: sprint( "type: TRAPFRAME SEGMENT" ); break;
      case SYSTEM_SEGMENT: sprint( "type: USER KERNEL STACK SEGMENT" ); break;
    }
    sprint( ", mapped to pa:%lx\n", lookup_pa(proc->pagetable, proc->mapped_info[i].va) );
  }
}


/* --- 堆增长 --- */
uint64 user_heap_grow(pagetable_t pagetable,uint64 old_size,uint64 new_size){
    // 为虚拟地址[old_size, new_size]分配页面并进行映射
    if(old_size >= new_size) return old_size; //非法，新的size应该大于旧的size
    old_size = ROUNDUP(old_size,PGSIZE); //向上对齐
    for(uint64 i = old_size; i < new_size; i += PGSIZE){ // 为[old_size, new_size]分配页面并进行映射
        char * mem_new = (char *)alloc_page();
        if(mem_new == NULL) panic("user_heap_malloc: out of memory");
        memset(mem_new, 0, PGSIZE);
        user_vm_map(pagetable, i, PGSIZE, (uint64)mem_new, prot_to_type(PROT_READ | PROT_WRITE,1));
    }
    return new_size;
}
/* -- 新分配用户堆空间 -- */
void user_better_malloc(uint64 n){
    // 在堆中新分配n个字节的内存
    if(n<0) panic("user_better_malloc: invalid size");
    uint64 new_size = current->heap_size + n;
    user_heap_grow(current->pagetable, current->heap_size, new_size);
    current->heap_size = new_size;
}


/* -- 内存池的初始化 -- */
void mcb_init(){
    if(init_flag==0){ //未被初始化
        current->heap_size = USER_FREE_ADDRESS_START;
        uint64 start = current->heap_size;
        user_better_malloc(sizeof(mem_control_block)); //分配第一个内存控制块
        pte_t *pte = page_walk(current->pagetable, start, 0);//找到第一个内存控制块的pte
        mem_control_block *first_control_block = (mem_control_block *) PTE2PA(*pte); //找到第一个内存控制块的地址
        current->mcb_head = (uint64)first_control_block; //记录第一个内存控制块的地址
        first_control_block->next = first_control_block; //初始化链表
        first_control_block->size = 0; //size初始为0
        current->mcb_tail = (uint64)first_control_block; //记录最后一个内存控制块的地址
        init_flag = 1;
    }
}

/*-- 内存分配 --*/
// 先从内存池（mcb_list)中找到合适的内存块，如果找到了就直接返回，如果没有找到就调用user_better_malloc分配新的内存块
uint64 malloc(int n){
    /** Step1:从内存池中查找可分配的空闲空间 **/
    mcb_init();
    mem_control_block * head = (mem_control_block *)current->mcb_head;
    mem_control_block * tail = (mem_control_block *)current->mcb_tail;

//    Debug  内存控制块链表的遍历
//    mem_control_block  * p = head;
//    while(1){
//        sprint("block: offset: %d, size: %d, is_available: %d\n", p->offset, p->size, p->is_available);
//        if(p->next == tail) break;
//        p = p->next;
//    }
//    sprint("\n");
    while(1){//从头到尾遍历内存控制块链表，找到第一个可用的内存块
//        sprint("block: offset: %d, size: %d, is_available: %d\n", head->offset, head->size, head->is_available);
        if(head->size >= n && head->is_available == 1){
//            sprint("malloc: find a available memory block, offset: %d, size: %d\n", head->offset, head->size);
            head->is_available = 0;
//            sprint("malloc: find a available memory block, offset: %d, size: %d\n", head->offset, head->size);
            return head->offset + sizeof(mem_control_block);
        }
        if(head->next == tail) break;
        head = head->next;
    }
    /** Step2:内存池没有满足要求的空闲，新分配空间 **/
    uint64 allocate_addr = current->heap_size; //新分配空间首地址
    user_better_malloc((uint64)(sizeof(mem_control_block) + n + 8)); //分配新的内存块
    pte_t * pte = page_walk(current->pagetable, allocate_addr, 0); //找到新分配的内存块的pte
    mem_control_block * new_control_block = (mem_control_block *)(PTE2PA(*pte) + (allocate_addr & 0xfff)); //找到新分配的内存块的地址
    uint64 align = (8-((uint64)new_control_block % 8))%8; //对齐的offset
    new_control_block = (mem_control_block *)((uint64)new_control_block + align); //对齐
    new_control_block->is_available = 0; //设置为不可用
    new_control_block->offset = allocate_addr; //设置offset
    new_control_block->size = n; //设置size
    new_control_block->next = head->next; //插入到链表中
    head->next = new_control_block;
    head = (mem_control_block *)current->mcb_head;
//    current->mcb_tail = (uint64)new_control_block;
//    sprint("malloc: allocate a new memory block, offset: %d, size: %d\n", new_control_block->offset, new_control_block->size);
    return allocate_addr + sizeof(mem_control_block);//返回新分配的空间首地址
}
void free(void* va){

    va = (void *)((uint64)va - sizeof(mem_control_block));
    pte_t *pte = page_walk(current->pagetable, (uint64)(va), 0);
    mem_control_block *cur = (mem_control_block *)(PTE2PA(*pte) + ((uint64)va & 0xfff));
    uint64 amo = (8 - ((uint64)cur % 8))%8;
    cur = (mem_control_block *)((uint64)cur + amo);
    if(cur->is_available == 1)
        panic("in free function, the memory has been freed before! \n");
    cur->is_available =1;
//    sprint("free: free a memory block, offset: %d, size: %d\n", cur->offset, cur->size);
}