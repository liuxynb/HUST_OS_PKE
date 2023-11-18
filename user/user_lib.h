/*
 * header file to be used by applications.
 */

int printu(const char *s, ...);
int exit(int code);
void *naive_malloc();
void naive_free(void *va);
int fork();
void yield();
// added @lab3_challenge1
int wait(int pid);
