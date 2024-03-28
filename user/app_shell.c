#include "user_lib.h"
#include "string.h"
#include "util/types.h"

int main(int argc, char *argv[]) {
  wait(1);
  printu("\n======== HUST-PKE Shell Start ========\n\n");
  char command[128] = {0}, para[128] = {0}, cwd[128] = {0}, username[128] = "root";
  memset(command, 128, sizeof(char));
  memset(para, 128, sizeof(char));
  
  while (1) {
    pwd(cwd);
    printu("[%s@localhost %s]$ ", username, cwd);
    scanu("%s %s", command, para);
    if (strcmp("q", command) == 0) break;
    else if (strcmp("pwd", command) == 0) { pwd(cwd); printu("%s\n", cwd); }
    else if (strcmp("cd", command) == 0) cd(para);
    else if (strcmp("mkdir", command) == 0) exec("/bin/mkdir", para);
    else if (strcmp("touch", command) == 0) exec("/bin/touch", para);
    else if (strcmp("echo", command) == 0) exec("/bin/echo", para);
    else if (strcmp("cat", command) == 0) exec("/bin/cat", para);
    else if (strcmp("ls", command) == 0) exec("/bin/ls", para);
    else if (strcmp("cow", command) == 0) exec("/bin/cow ", para);
    else if (strcmp("backtrace", command) == 0) exec("/bin/backtrace", para);
    else if (strcmp("errorline", command) == 0) exec("/bin/errorline", para);
    else if (strcmp("sem", command) == 0) exec("/bin/sem", para);
    else if (strcmp("pagefault", command) == 0) exec("/bin/pagefault", para);
    else if (strcmp("END", command) == 0) break;
    else printu("Command not found: %s\n", command);
  }
  exit(0);
  return 0;
}
