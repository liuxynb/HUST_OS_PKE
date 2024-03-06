#ifndef _SYNC_UTILS_H_
#define _SYNC_UTILS_H_

static inline void sync_barrier(volatile int *counter, int all) {

  int local;

  asm volatile("amoadd.w %0, %2, (%1)\n"
               : "=r"(local)
               : "r"(counter), "r"(1)
               : "memory");

  if (local + 1 < all) {
    do {
      asm volatile("lw %0, (%1)\n" : "=r"(local) : "r"(counter) : "memory");
    } while (local < all);
  }
}

// added @lab2_c3, for isolation of page allocation and free operations using amoswap
// !reference https://github.com/uniqueFranky/riscv-pke/
// volatile关键字用于防止编译器优化， 防止优化编译器把变量从内存装入 CPU 寄存器中
static inline void spin_lock(volatile int *lock) {
  int local = 0;
  do {
    asm volatile("amoswap.w %0, %2, (%1)"
                : "=r"(local)
                : "r"(lock), "r"(1)
                : "memory");
  } while(local);
}

static inline void spin_unlock(volatile int *lock) {
  asm volatile("sw zero, (%0)"
              :
              : "r"(lock)
              : "memory");
}
#endif