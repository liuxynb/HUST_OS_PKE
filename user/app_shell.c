/*
 * This app starts a very simple shell and executes some simple commands.
 * The commands are stored in the hostfs_root/shellrc
 * The shell loads the file and executes the command line by line.                 
 */
#include "user_lib.h"
#include "string.h"
#include "util/types.h"

int main(int argc, char *argv[]) {
  printu("\n======== HUST-PKE Shell Start ========\n\n");
  char command[128], para[128], cwd[128];
  char username[128] = "root";
  char password[128] = "123456";
  // char input_username[128], input_password[128];
  // int cnt = 0;
  // while(1){
  //   printu("Please input username: ");
  //   scanu("%s", input_username);
  //   if(strcmp(input_username, username) != 0){
  //     printu("Username not exist!\n");
  //     if(++cnt >= 3){
  //       printu("You have tried too many times!\n");
  //       exit(0);
  //     }
  //   }
  //   else{
  //     break;
  //   }
  // }
  // cnt = 0;
  // while(1){
  //   printu("Please input password: ");
  //   scanu("%s", input_password);
  //   if(strcmp(input_password, password) != 0){
  //     printu("Password not correct!\n");
  //     if(++cnt >= 3){
  //       printu("You have tried too many times!\n");
  //       exit(0);
  //     }
  //   }
  //   else{
  //     break;
  //   }
  // }
  
  // printu("\n======== Login Success:Last login: Wed Mar 13 08:43:43 2024 from 10.21.184.196========\n\n");
  while (1){
    pwd(cwd);
    printu("[%s@localhost %s]$ ", username,cwd);
    scanu("%s %s", command, para);
    if (strcmp("q", command) == 0) // quit shell
    {
      break;
    }else if(strcmp("pwd", command) == 0){
      pwd(cwd);
      printu("%s\n", cwd);
    }else if(strcmp("cd", command) == 0){
      cd(para);
    }else{
      int pid = fork();
      if(pid == 0) {
        int ret = exec(command, para);
        return ret;
      }
      else
      {
        wait(pid);
      }
    }
  }
  exit(0);
  return 0;
}
