#include "user_lib.h"
#include "util/string.h"
#include "util/types.h"

void ls(char *path) {
  int dir_fd = opendir_u(path);
  printu("------------------------------\n");
  printu("ls \"%s\":\n", path);
  printu("[name]               [inode_num]\n");
  struct dir dir;
  int width = 20;
  while(readdir_u(dir_fd, &dir) == 0) {
    // we do not have %ms :(
    char name[width + 1];
    memset(name, ' ', width + 1);
    name[width] = '\0';
    if (strlen(dir.name) < width) {
      strcpy(name, dir.name);
      name[strlen(dir.name)] = ' ';
      printu("%s %d\n", name, dir.inum);
    }
    else
      printu("%s %d\n", dir.name, dir.inum);
  }
  printu("------------------------------\n");
  closedir_u(dir_fd);
}

int main(int argc, char *argv[]) {
  int MAXBUF = 512;
  char str[] = "hello world";
  char buf[MAXBUF];
  int fd1, fd2;
  
  printu("\n======== establish the file ========\n");

  fd1 = open("/RAMDISK0/ramfile", O_RDWR | O_CREAT);
  printu("create file: /RAMDISK0/ramfile\n");
  close(fd1);

  printu("\n======== Test 1: hard link ========\n");

  link_u("/RAMDISK0/ramfile", "/RAMDISK0/ramfile2");
  printu("create hard link: /RAMDISK0/ramfile2 -> /RAMDISK0/ramfile\n");

  fd1 = open("/RAMDISK0/ramfile", O_RDWR);
  fd2 = open("/RAMDISK0/ramfile2", O_RDWR);
  
  printu("file descriptor fd1 (ramfile): %d\n", fd1);
  printu("file descriptor fd2 (ramfile2): %d\n", fd2);

  // check the number of hard links to ramfile on disk
  struct istat st;
  disk_stat_u(fd1, &st);
  printu("ramfile hard links: %d\n", st.st_nlinks);
  if (st.st_nlinks != 2) {
    printu("ERROR: the number of hard links to ramfile should be 2, but it is %d\n",
             st.st_nlinks);
    exit(-1);
  }
  
  write_u(fd1, str, strlen(str));
  printu("/RAMDISK0/ramfile write content: \n%s\n", str);

  read_u(fd2, buf, MAXBUF);
  printu("/RAMDISK0/ramfile2 read content: \n%s\n", buf);

  close(fd1);
  close(fd2);

  printu("\n======== Test 2: unlink ========\n");

  ls("/RAMDISK0");

  unlink_u("/RAMDISK0/ramfile");
  printu("unlink: /RAMDISK0/ramfile\n");

  ls("/RAMDISK0");

  // check the number of hard links to ramfile2 on disk
  fd2 = open("/RAMDISK0/ramfile2", O_RDWR);
  disk_stat_u(fd2, &st);
  printu("ramfile2 hard links: %d\n", st.st_nlinks);
  if (st.st_nlinks != 1) {
    printu("ERROR: the number of hard links to ramfile should be 1, but it is %d\n",
             st.st_nlinks);
    exit(-1);
  }
  close(fd2);

  printu("\nAll tests passed!\n\n");
  exit(0);
  return 0;
}
