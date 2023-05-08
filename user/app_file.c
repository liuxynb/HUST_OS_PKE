#include "user_lib.h"
#include "util/string.h"
#include "util/types.h"

int main(int argc, char *argv[]) {
  int fd;
  int MAXBUF = 512;
  char buf[MAXBUF];
  char str[] = "hello world";
  int fd1, fd2;

  printu("\n======== Test 1: read host file  ========\n");
  printu("read: /hostfile.txt\n");

  fd = open("/hostfile.txt", O_RDONLY);
  printu("file descriptor fd: %d\n", fd);

  read_u(fd, buf, MAXBUF);
  printu("read content: \n%s\n", buf);

  close(fd);

  printu("\n======== Test 2: create/write rfs file ========\n");
  printu("write: /RAMDISK0/ramfile\n");

  fd = open("/RAMDISK0/ramfile", O_RDWR | O_CREAT);
  printu("file descriptor fd: %d\n", fd);

  write_u(fd, buf, strlen(buf));
  printu("write content: \n%s\n", buf);
  close(fd);

  printu("\n======== Test 3: read rfs file ========\n");
  printu("read: /RAMDISK0/ramfile\n");

  fd = open("/RAMDISK0/ramfile", O_RDWR);
  printu("file descriptor fd: %d\n", fd);

  read_u(fd, buf, MAXBUF);
  printu("read content: \n%s\n", buf);
  close(fd);

  printu("\n======== Test 4: open twice ========\n");

  fd1 = open("/RAMDISK0/ramfile", O_RDWR | O_CREAT);
  fd2 = open("/RAMDISK0/ramfile", O_RDWR | O_CREAT);

  printu("file descriptor fd1(ramfile): %d\n", fd1);
  printu("file descriptor fd2(ramfile): %d\n", fd2);

  write_u(fd1, str, strlen(str));
  printu("write content: \n%s\n", str);

  read_u(fd2, buf, MAXBUF);
  printu("read content: \n%s\n", buf);

  close(fd1);
  close(fd2);

  printu("\nAll tests passed!\n\n");
  exit(0);
  return 0;
}
