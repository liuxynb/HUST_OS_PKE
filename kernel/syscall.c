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
#include "pmm.h"
#include "vmm.h"
#include "sched.h"
#include "proc_file.h"
#include "elf.h"
#include "spike_interface/spike_file.h"

#include "spike_interface/spike_utils.h"


extern struct elf_sym_table elf_sym_tab;

static void transfer_2_absolute_path(char * relative_path,char * absolute_path){
    /**
     * 考虑两种相对路径：'.' - 1 和'..' - 2
     * 通过获取当前进程的cwd，然后逐级向上查找，直到根目录，得到父目录的绝对路径
     * 在和相对路径拼接，得到绝对路径
     */
//     sprint("relative_path:%s\n",relative_path);
    if(relative_path[0] == '/'){
        memcpy(absolute_path,relative_path,strlen(relative_path));
        return;
    }else{
      sprint("the relative path is not start with '/'\n");
    }
    int type = 0;
    struct dentry * cwd = current->pfiles->cwd;
    // sprint("***cwd:%s***\n",cwd->name);
    if(relative_path[0] == '.'){
        type = 1;
        if(relative_path[1] == '.'){//相对路径为'../*'，则需要返回上一级目录
            type = 2;
            cwd = cwd->parent;
        }
    }
    for(;cwd;cwd=cwd->parent){
        char tmp[MAX_DEVICE_NAME_LEN];
        memset(tmp,0,MAX_DEVICE_NAME_LEN);
        memcpy(tmp,absolute_path, strlen(absolute_path));
        memset(absolute_path,0,MAX_DEVICE_NAME_LEN);
        memcpy(absolute_path,cwd->name,strlen(cwd->name));
        if(cwd->parent){//如果不是根目录，则需要加上'/'
            absolute_path[strlen(cwd->name)] = '/';
            absolute_path[strlen(cwd->name)+1] = '\0';
        }
        strcat(absolute_path,tmp);
//        sprint("absolute_path:%s\n",absolute_path);
    }
    switch (type) {
        case 1:
            strcat(absolute_path,relative_path+2);//相对路径为'./'，则需要去掉'./'
            break;
        case 2:
            strcat(absolute_path,relative_path+3);//相对路径为'../'，则需要去掉'../'
            break;
        default:
            // sprint("absolute_path:%s\n",absolute_path);
            // sprint("relative_path:%s\n",relative_path);
            strcat(absolute_path,relative_path);//路径为'/'，则不需要去掉
            break;
    }
}


//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char* buf, size_t n) {
  // buf is now an address in user space of the given app's user stack,
  // so we have to transfer it into phisical address (kernel is running in direct mapping).
  assert( current );
  char* pa = (char*)user_va_to_pa((pagetable_t)(current->pagetable), (void*)buf);
  sprint(pa);
  return 0;
}

//
// implement the SYS_user_exit syscall
//
ssize_t sys_user_exit(uint64 code) {
  sprint("User exit with code:%d.\n", code);
  // sprint("process %d exit.\n", current->pid);
  // reclaim the current process, and reschedule. added @lab3_1
  free_process( current );
  schedule();
  return 0;
}

//
// maybe, the simplest implementation of malloc in the world ... added @lab2_2
//
uint64 sys_user_allocate_page(uint64 n) {
  // void* pa = alloc_page();
  // uint64 va;
  // // if there are previously reclaimed pages, use them first (this does not change the
  // // size of the heap)
  // if (current->user_heap.free_pages_count > 0) {
  //   va =  current->user_heap.free_pages_address[--current->user_heap.free_pages_count];
  //   assert(va < current->user_heap.heap_top);
  // } else {
  //   // otherwise, allocate a new page (this increases the size of the heap by one page)
  //   va = current->user_heap.heap_top;
  //   current->user_heap.heap_top += PGSIZE;

  //   current->mapped_info[HEAP_SEGMENT].npages++;
  // }
  // user_vm_map((pagetable_t)current->pagetable, va, PGSIZE, (uint64)pa,
  //        prot_to_type(PROT_WRITE | PROT_READ, 1));
  return malloc((int) n);
}

//
// reclaim a page, indicated by "va". added @lab2_2
//
uint64 sys_user_free_page(uint64 va) {
  // user_vm_unmap((pagetable_t)current->pagetable, va, PGSIZE, 1);
  // // add the reclaimed page to the free page list
  // current->user_heap.free_pages_address[current->user_heap.free_pages_count++] = va;
  free((void*)va);
  return 0;
}

//
// kerenl entry point of naive_fork
//
ssize_t sys_user_fork(){
  // sprint("User call fork.\n");
  return do_fork( current );
}

//
// kerenl entry point of yield. added @lab3_2
//
ssize_t sys_user_yield() {
  // TODO (lab3_2): implment the syscall of yield.
  // hint: the functionality of yield is to give up the processor. therefore,
  // we should set the status of currently running process to READY, insert it in
  // the rear of ready queue, and finally, schedule a READY process to run.
//  panic( "You need to implement the yield syscall in lab3_2.\n" );
    current->status = READY;
    insert_to_ready_queue(current);
    schedule();
  return 0;
}

//
// open file
//
ssize_t sys_user_open(char *pathva, int flags) {
  char* pathpa = (char*)user_va_to_pa((pagetable_t)(current->pagetable), pathva);
  return do_open(pathpa, flags);
}

//
// read file
//
ssize_t sys_user_read(int fd, char *bufva, uint64 count) {
  int i = 0;
  while (i < count) { // count can be greater than page size
    uint64 addr = (uint64)bufva + i;
    uint64 pa = lookup_pa((pagetable_t)current->pagetable, addr);
    uint64 off = addr - ROUNDDOWN(addr, PGSIZE);
    uint64 len = count - i < PGSIZE - off ? count - i : PGSIZE - off;
    uint64 r = do_read(fd, (char *)pa + off, len);
    i += r; if (r < len) return i;
  }
  return count;
}

//
// write file
//
ssize_t sys_user_write(int fd, char *bufva, uint64 count) {
  int i = 0;
  while (i < count) { // count can be greater than page size
    uint64 addr = (uint64)bufva + i;
    uint64 pa = lookup_pa((pagetable_t)current->pagetable, addr);
    uint64 off = addr - ROUNDDOWN(addr, PGSIZE);
    uint64 len = count - i < PGSIZE - off ? count - i : PGSIZE - off;
    uint64 r = do_write(fd, (char *)pa + off, len);
    i += r; if (r < len) return i;
  }
  return count;
}

//
// lseek file
//
ssize_t sys_user_lseek(int fd, int offset, int whence) {
  return do_lseek(fd, offset, whence);
}

//
// read vinode
//
ssize_t sys_user_stat(int fd, struct istat *istat) {
  struct istat * pistat = (struct istat *)user_va_to_pa((pagetable_t)(current->pagetable), istat);
  return do_stat(fd, pistat);
}

//
// read disk inode
//
ssize_t sys_user_disk_stat(int fd, struct istat *istat) {
  struct istat * pistat = (struct istat *)user_va_to_pa((pagetable_t)(current->pagetable), istat);
  return do_disk_stat(fd, pistat);
}

//
// close file
//
ssize_t sys_user_close(int fd) {
  return do_close(fd);
}

//
// lib call to opendir
//
ssize_t sys_user_opendir(char * pathva){
  char * pathpa = (char*)user_va_to_pa((pagetable_t)(current->pagetable), pathva);
  return do_opendir(pathpa);
}

//
// lib call to readdir
//
ssize_t sys_user_readdir(int fd, struct dir *vdir){
  struct dir * pdir = (struct dir *)user_va_to_pa((pagetable_t)(current->pagetable), vdir);
  return do_readdir(fd, pdir);
}

//
// lib call to mkdir
//
ssize_t sys_user_mkdir(char * pathva){
  sprint("user_s0:%lx\n",current->trapframe->regs.s0);
  char * pathpa = (char*)user_va_to_pa((pagetable_t)(current->pagetable), pathva);
  return do_mkdir(pathpa);
}

//
// lib call to closedir
//
ssize_t sys_user_closedir(int fd){
  return do_closedir(fd);
}

//
// lib call to link
//
ssize_t sys_user_link(char * vfn1, char * vfn2){
  char * pfn1 = (char*)user_va_to_pa((pagetable_t)(current->pagetable), (void*)vfn1);
  char * pfn2 = (char*)user_va_to_pa((pagetable_t)(current->pagetable), (void*)vfn2);
  return do_link(pfn1, pfn2);
}

//
// lib call to unlink
//
ssize_t sys_user_unlink(char * vfn){
  char * pfn = (char*)user_va_to_pa((pagetable_t)(current->pagetable), (void*)vfn);
  return do_unlink(pfn);
}

ssize_t sys_user_wait(int pid) {
  return do_wait(pid);
}
ssize_t sys_user_exec(char *path, char *para)
{
  // 将用户虚拟地址转换为物理地址
  char *ppath = (char *)user_va_to_pa((pagetable_t)(current->pagetable), (void *)path);
  char *ppara = (char *)user_va_to_pa((pagetable_t)(current->pagetable), (void *)para);
  char absolute_path[MAX_DEVICE_NAME_LEN*2];
  char absolute_para[MAX_DEVICE_NAME_LEN*2];
  transfer_2_absolute_path(ppath, absolute_path);
  transfer_2_absolute_path(ppara, absolute_para);
  // sprint("exec absolute path:%s && absolute para:%s\n",absolute_path,absolute_para);
  // 处理程序参数
  char para_new[100];
  strcpy(para_new, absolute_para);
  
  // 执行程序
  // sprint("*** exec ***\n");
  int ret = do_exec(absolute_path,absolute_para);
  if (ret < 0)
  {
    sprint("exec failed\n");
    return -1;
  }
  return 0;
}
ssize_t sys_user_scan(const char *buf)
{
  // buf is now an address in user space of the given app's user stack,
  // so we have to transfer it into phisical address (kernel is running in direct mapping).
  assert(current);
  char *pa = (char *)user_va_to_pa((pagetable_t)(current->pagetable), (void *)buf);
  int read_len = spike_file_read(stdin, pa, 256);
  return 0;
}

ssize_t sys_sem_new(int value){
    return do_sem_new(value);
}
ssize_t sys_sem_p(int sem_id){
    return do_sem_p(sem_id);
}
ssize_t sys_sem_v(int sem_id){
    return do_sem_v(sem_id);
}


//
// lib call to read cwd
//
ssize_t sys_user_read_cwd(char *pathva){
    char * pathpa = (char*)user_va_to_pa((pagetable_t)(current->pagetable), pathva);//得到物理地址z
    // sprint("pathpa:%s\n",pathpa);
    return do_read_cwd(pathpa);
}

//
//lib call to change cwd
//
ssize_t sys_user_change_cwd(char *pathva){
    char * pathpa = (char*)user_va_to_pa((pagetable_t)(current->pagetable), pathva);
    return do_change_cwd(pathpa);
}
int print_func_name(uint64 ret_addr) {
  // sprint("ret_addr:%lx\n",ret_addr);
    for(int i=0;i<elf_sym_tab.sym_count;i++){
        if (ret_addr >= elf_sym_tab.sym[i].st_value && ret_addr < elf_sym_tab.sym[i].st_value + elf_sym_tab.sym[i].st_size) {
            sprint("%s\n", elf_sym_tab.sym_names[i]);
            if (strcmp(elf_sym_tab.sym_names[i], "main") == 0)//到main函数就返回
                return 0;
            return 1;
        }
    }
    return 1;
}
//added lab1_ch1
ssize_t sys_user_print_backtrace(uint64 n) {
//  sprint("sp:%lx,s0:%lx", current->trapframe->regs.sp, current->trapframe->regs.s0);
  // sprint("print backtrace:user_s0=%lx,user_sp=%lx\n",current->trapframe->regs.s0,current->trapframe->regs.sp);
   uint64 user_s0 = current->trapframe->regs.s0;//得到s0
    uint64 user_sp = user_s0;//sp = s0，得到用户态的sp
    uint64 user_ra = user_sp + 8; //ra = sp + 8，得到用户态的ra
    for (int i=0;i<n;i++){
    //  sprint("backtrace %d: ra:%p\n",i,*((uint64 *)user_ra));
        void * addr = user_va_to_pa((pagetable_t)(current->pagetable), (void *)user_ra);//将用户态的地址转换为物理地址
        // sprint("backtrace %d: ra:%lx\n",i,*(uint64 *)addr);
        if(print_func_name(*(uint64 * )addr)==0)//将ra指向的地址转换为uint64，然后调用print_func_name
        return i;

      user_ra = user_ra + 16;//每次移动2个字长
    }
    return 0;
}

//
// [a0]: the syscall number; [a1] ... [a7]: arguments to the syscalls.
// returns the code of success, (e.g., 0 means success, fail for otherwise)
//
long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
  switch (a0) {
    case SYS_user_print:
      return sys_user_print((const char*)a1, a2);
    case SYS_user_exit:
      return sys_user_exit(a1);
    // added @lab2_2
    case SYS_user_allocate_page:
      return sys_user_allocate_page(a1);
    case SYS_user_free_page:
      return sys_user_free_page(a1);
    case SYS_user_fork:
      return sys_user_fork();
    case SYS_user_yield:
      return sys_user_yield();
    // added @lab4_1
    case SYS_user_open:
      return sys_user_open((char *)a1, a2);
    case SYS_user_read:
      return sys_user_read(a1, (char *)a2, a3);
    case SYS_user_write:
      return sys_user_write(a1, (char *)a2, a3);
    case SYS_user_lseek:
      return sys_user_lseek(a1, a2, a3);
    case SYS_user_stat:
      return sys_user_stat(a1, (struct istat *)a2);
    case SYS_user_disk_stat:
      return sys_user_disk_stat(a1, (struct istat *)a2);
    case SYS_user_close:
      return sys_user_close(a1);
    // added @lab4_2
    case SYS_user_opendir:
      return sys_user_opendir((char *)a1);
    case SYS_user_readdir:
      return sys_user_readdir(a1, (struct dir *)a2);
    case SYS_user_mkdir:
      return sys_user_mkdir((char *)a1);
    case SYS_user_closedir:
      return sys_user_closedir(a1);
    // added @lab4_3
    case SYS_user_link:
      return sys_user_link((char *)a1, (char *)a2);
    case SYS_user_unlink:
      return sys_user_unlink((char *)a1);
    case SYS_user_wait:
      return sys_user_wait(a1);
    case SYS_user_exec:
      return sys_user_exec((char *)a1, (char *)a2);
    case SYS_user_scanf:
      return sys_user_scan((const char*)a1);
    case SYS_user_read_cwd:
      return sys_user_read_cwd((char *)a1);
    case SYS_user_change_cwd:
      return sys_user_change_cwd((char *)a1);
    case SYS_user_sem_new:
      return sys_sem_new(a1);
    case SYS_user_sem_p:
      return sys_sem_p(a1);
    case SYS_user_sem_v:
      return sys_sem_v(a1);
    case SYS_user_print_backtrace:
      return sys_user_print_backtrace(a1);

    default:
      panic("Unknown syscall %ld \n", a0);
  }
}
