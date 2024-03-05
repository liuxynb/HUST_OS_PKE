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
static inline void spin_lock(volatile int *lock) {
    int expected = 0;
    int desired = 1;
    asm volatile(
        "1:\n\t"
        "amoswap.w %0, %2, (%1)\n\t"
        "bnez %0, 1b\n\t"
        : "+r" (expected)
        : "r" (lock), "r" (desired)
        : "memory"
    );
}

static inline void spin_unlock(volatile int *lock) {
    asm volatile(
        "sw zero, (%0)"
        :
        : "r" (lock)
    );
}

#endif