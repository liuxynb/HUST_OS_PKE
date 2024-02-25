/*
 * This app starts a very simple shell and executes some simple commands.
 * The commands are stored in the hostfs_root/shellrc
 * The shell loads the file and executes the command line by line.                 
 */
#include "user_lib.h"
#include "string.h"
#include "util/types.h"

int main(int argc, char *argv[]) {
  printu("\n======== Shell Start ========\n\n");
  int fd;
  int MAXBUF = 1024;
  char buf[MAXBUF];
  char *token;
  char delim[3] = " \n";
  fd = open("/shellrc", O_RDONLY);

  read_u(fd, buf, MAXBUF);
  close(fd);
  char *command = naive_malloc();
  char *para = naive_malloc();
  int start = 0;
  while (1)
  {
    if(!start) {
      token = strtok(buf, delim);
      start = 1;
    }
    else 
      token = strtok(NULL, delim);
    strcpy(command, token);
    token = strtok(NULL, delim);
    strcpy(para, token);
    if(strcmp(command, "END") == 0 && strcmp(para, "END") == 0)
      break;
    printu("Next command: %s %s\n\n", command, para);
    printu("==========Command Start============\n\n");
    int pid = fork();
    if(pid == 0) {
      int ret = exec(command, para);
      if (ret == -1)
      printu("exec failed!\n");
    }
    else
    {
      wait(pid);
      printu("==========Command End============\n\n");
    }
  }
  exit(0);
  return 0;
}
