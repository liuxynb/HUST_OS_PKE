/*
* This app create two child process.
* Use semaphores to control the order of
* the main process and two child processes print info. 
*/
#include "user/user_lib.h"
#include "util/types.h"

int main(void) {
    int main_sem, child_sem[2];// 三个信号量，分别给main进程，子进程Child0,以及孙子进程Child1使用
    main_sem = sem_new(1);  // main_sem 初始化为1
    for (int i = 0; i < 2; i++) child_sem[i] = sem_new(0); // child_sem 初始化为0
    int pid = fork();// 创建子进程 Child0
    if (pid == 0) {
        // 子进程执行代码段
        pid = fork();// 创建孙子进程 Child1
        for (int i = 0; i < 10; i++) {
            sem_P(child_sem[pid == 0]);//P操作
            printu("Child%d print %d\n", pid == 0, i);
            if (pid != 0) sem_V(child_sem[1]); else sem_V(main_sem);
        }
    } else {
        //父进程执行代码段
        for (int i = 0; i < 10; i++) {
            sem_P(main_sem);
            printu("Parent print %d\n", i);
            sem_V(child_sem[0]);
        }
    }
    exit(0);
    return 0;
}
