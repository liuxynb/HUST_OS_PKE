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
  char str[] = "hello world";
  int fd;
  
  printu("\n======== Test 1: open and read dir ========\n");

  ls("/RAMDISK0");
  
  printu("\n======== Test 2: make dir ========\n");

  mkdir_u("/RAMDISK0/sub_dir");
  printu("make: /RAMDISK0/sub_dir\n");

  ls("/RAMDISK0");

  // try to write a file in the new dir
  printu("write: /RAMDISK0/sub_dir/ramfile\n");

  fd = open("/RAMDISK0/sub_dir/ramfile", O_RDWR | O_CREAT);
  printu("file descriptor fd: %d\n", fd);

  write_u(fd, str, strlen(str));
  printu("write content: \n%s\n", str);

  close(fd);

  ls("/RAMDISK0/sub_dir");

  printu("\nAll tests passed!\n\n");
  exit(0);
  return 0;
}
