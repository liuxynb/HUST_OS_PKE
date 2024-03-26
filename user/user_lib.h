/*
 * header file to be used by applications.
 */

#ifndef _USER_LIB_H_
#define _USER_LIB_H_
#include "util/types.h"
#include "kernel/proc_file.h"

int printu(const char *s, ...);
int exit(int code);

int fork();
void yield();

// added @ lab4_1
int open(const char *pathname, int flags);
int read_u(int fd, void *buf, uint64 count);
int write_u(int fd, void *buf, uint64 count);
int lseek_u(int fd, int offset, int whence);
int stat_u(int fd, struct istat *istat);
int disk_stat_u(int fd, struct istat *istat);
int close(int fd);

// added @ lab4_2
int opendir_u(const char *pathname);
int readdir_u(int fd, struct dir *dir);
int mkdir_u(const char *pathname);
int closedir_u(int fd);

// added @ lab4_3
int link_u(const char *fn1, const char *fn2);
int unlink_u(const char *fn);
int exec(char * command,char * para);
int wait(int pid);

//added @challengeX
int scanu(const char *s, ...);

//added cd & pwd
void pwd(char * path);
void cd(const char *path);

//added lab3_ch2
int sem_new(int value);
int sem_P(int sem_id);
int sem_V(int sem_id);

//added lab1_ch1
int print_backtrace(int n);

//added lab2_ch2
void * malloc(uint64 size);
void free(void * ptr);
#endif
