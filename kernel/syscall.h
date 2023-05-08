/*
 * define the syscall numbers of PKE OS kernel.
 */
#ifndef _SYSCALL_H_
#define _SYSCALL_H_

// syscalls of PKE OS kernel. append below if adding new syscalls.
#define SYS_user_base 64
#define SYS_user_print (SYS_user_base + 0)
#define SYS_user_exit (SYS_user_base + 1)
// added @lab2_2
#define SYS_user_allocate_page (SYS_user_base + 2)
#define SYS_user_free_page (SYS_user_base + 3)
// added @lab3_1
#define SYS_user_fork (SYS_user_base + 4)
#define SYS_user_yield (SYS_user_base + 5)
// added @lab4_1
#define SYS_user_open (SYS_user_base + 17)
#define SYS_user_read (SYS_user_base + 18)
#define SYS_user_write (SYS_user_base + 19)
#define SYS_user_lseek (SYS_user_base + 20)
#define SYS_user_stat (SYS_user_base + 21)
#define SYS_user_disk_stat (SYS_user_base + 22)
#define SYS_user_close (SYS_user_base + 23)
// added @lab4_2
#define SYS_user_opendir  (SYS_user_base + 24)
#define SYS_user_readdir  (SYS_user_base + 25)
#define SYS_user_mkdir    (SYS_user_base + 26)
#define SYS_user_closedir (SYS_user_base + 27)
// added @lab4_3
#define SYS_user_link   (SYS_user_base + 28)
#define SYS_user_unlink (SYS_user_base + 29)

long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7);

#endif
