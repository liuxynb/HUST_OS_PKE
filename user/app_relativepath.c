#include "user_lib.h"
#include "util/string.h"
#include "util/types.h"

void pwd() {
  char path[30];
  read_cwd(path);
  printu("cwd:%s\n", path);
}

void cd(const char *path) {
  if (change_cwd(path) != 0)
    printu("cd failed\n");
}

int main(int argc, char *argv[]) {
  int fd;
  int MAXBUF = 512;
  char buf[MAXBUF];
  char str[] = "hello world";
  int fd1, fd2;

  printu("\n======== Test 1: change current directory  ========\n");

  pwd();
  cd("./RAMDISK0");
  printu("change current directory to ./RAMDISK0\n");
  pwd();

  printu("\n======== Test 2: write/read file by relative path  ========\n");
  printu("write: ./ramfile\n");

  fd = open("./ramfile", O_RDWR | O_CREAT);
  printu("file descriptor fd: %d\n", fd);

  write_u(fd, str, strlen(str));
  printu("write content: \n%s\n", str);
  close(fd);

  fd = open("./ramfile", O_RDWR);
  printu("read: ./ramfile\n");

  read_u(fd, buf, MAXBUF);
  printu("read content: \n%s\n", buf);
  close(fd);

  printu("\n======== Test 3: Go to parent directory  ========\n");

  pwd();
  cd("..");
  printu("change current directory to ..\n");
  pwd();
  
  printu("read: ./hostfile.txt\n");

  fd = open("./hostfile.txt", O_RDONLY);
  printu("file descriptor fd: %d\n", fd);

  read_u(fd, buf, MAXBUF);
  printu("read content: \n%s\n", buf);

  close(fd);

  printu("\nAll tests passed!\n\n");
  exit(0);
  return 0;
}
