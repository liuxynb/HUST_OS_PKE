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

static inline void spin_lock(volatile int *lock) {
  
  int local;
     asm volatile("lw %0, (%1)\n"
                     : "=r"(local)
                     : "r"(lock)
                     : "memory");
        // Spin until semaphore value becomes non-negative
    while (local <= 0) {
        // Load semaphore value
        asm volatile("lw %0, (%1)\n"
                     : "=r"(local)
                     : "r"(lock)
                     : "memory");
    }
    // Decrease semaphore value by 1 using atomic amoadd.w instruction
    asm volatile("amoadd.w %0, %2, (%1)\n"
                 : "=r"(local)
                 : "r"(lock), "r"(-1)
                 : "memory");


}


static inline void spin_unlock(volatile int *lock) {
  int local;

    // Increase semaphore value by 1 using atomic amoadd.w instruction
    asm volatile("amoadd.w %0, %2, (%1)\n"
                 : "=r"(local)
                 : "r"(lock), "r"(1)
                 : "memory");
}

#endif