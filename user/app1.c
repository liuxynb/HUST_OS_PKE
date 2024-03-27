#include "user_lib.h"
#include "util/types.h"

#define N 5
#define BASE 5

int main(void) {
  void *p[N];

  for (int i = 0; i < N; i++) {
    p[i] = malloc(4096);
    int *pi = p[i];
    *pi = BASE + i;
    printu(">>> user alloc 1 @ vaddr 0x%x\n", p[i]);
  }

  for (int i = 0; i < N; i++) {
    int *pi = p[i];
    printu(">>> user 1: %d\n", *pi);
    free(p[i]);
  }

  exit(0);
}