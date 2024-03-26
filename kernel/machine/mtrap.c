#include "kernel/riscv.h"
#include "kernel/process.h"
#include "spike_interface/spike_utils.h"
#include "util/string.h"

static void handle_instruction_access_fault() { panic("Instruction access fault!"); }

static void handle_load_access_fault() { panic("Load access fault!"); }

static void handle_store_access_fault() { panic("Store/AMO access fault!"); }

static void handle_illegal_instruction() { 
  panic("Illegal instruction!"); 
}

static void handle_misaligned_load() { panic("Misaligned Load!"); }

static void handle_misaligned_store() { panic("Misaligned AMO!"); }

// added @lab1_3
static void handle_timer() {
  int cpuid = 0;
  // setup the timer fired at next time (TIMER_INTERVAL from now)
  *(uint64*)CLINT_MTIMECMP(cpuid) = *(uint64*)CLINT_MTIMECMP(cpuid) + TIMER_INTERVAL;

  // setup a soft interrupt in sip (S-mode Interrupt Pending) to be handled in S-mode
  write_csr(sip, SIP_SSIP);
}

void print_errorline(){
    // output sample:Runtime error at user/app_errorline.c:13 + code_line
    char file_name[100];
    char code_line[1000];
    memset(file_name, 0, 100);
    memset(code_line, 0, 1000);
    
    uint64 epc = read_csr(mepc);//读取导致异常的指令地址
    sprint("epc:%p\n",epc);
    // sprint("line_ind:%d\n",current->line_ind);
    for(int i = 0; i < current->line_ind; i++){
        // sprint("i:%d , addr:%p , epc:%p\n",i,current->line[i].addr,epc);
        if(current->line[i].addr > epc){//找到导致异常的指令地址所在的行i-1
          sprint("i:%d\n",i);
          // 根据出错的行数，找到对应的文件具体名称
          // 完整文件名 = 目录名(在dir中) + 文件名(在file中)
          addr_line *line = &current->line[i-1];
          
          
          uint64 dir_idx = current->file[line->file].dir;
          sprint("dir_idx:%d\n",dir_idx);
          // panic("Runtime error at unknown location1\n");
          // panic("print_errorline\n");
          strcpy(file_name, current->dir[current->file[line->file].dir]);
          // panic("print_errorline\n");
          // panic("Runtime error at unknown location2\n");
          sprint("file_name:%s\n",file_name);
          // panic("Runtime error at unknown location2\n");
          // 将文件名拼接到file_name后面，/分割
          // panic("Runtime error at unknown location3\n");
          file_name[strlen(file_name)] = '/';
          
          strcpy(file_name + strlen(file_name), current->file[line->file].file);
          
          sprint("file_name:%s\n",file_name);
          // panic("Runtime error at unknown location4\n");
          // 根据出错的行数，找到对应的代码行
          spike_file_t *f = spike_file_open(file_name, O_RDONLY,0);//打开文件
          struct stat st;
          spike_file_stat(f, &st);
          spike_file_read(f, code_line, st.st_size);
          spike_file_close(f);
          // 找到对应的代码行
          int offset = 0,cnt = 0;
          sprint("line:%d\n",line->line);
          while(offset < st.st_size){
              int x = offset;
              while(x < st.st_size && code_line[x] != '\n')x++;
              if(cnt == line->line-1){
                  code_line[x] = 0;
                  sprint("Runtime error at %s:%d\n%s\n", file_name, line->line, code_line + offset);
                  break;
              } else {
                  cnt++;
                  offset = x + 1;
              }
          }
              break;
        }else{
          // sprint("line:%d continue\n",current->line[i].line);
        }
    }
}

//
// handle_mtrap calls a handling function according to the type of a machine mode interrupt (trap).
//
void handle_mtrap() {
  uint64 mcause = read_csr(mcause);
  switch (mcause) {
    case CAUSE_MTIMER:
      sprint("handle_mtrap: CAUSE_MTIMER\n");
      print_errorline();
      handle_timer();
      break;
    case CAUSE_FETCH_ACCESS:
      sprint("handle_mtrap: CAUSE_FETCH_ACCESS\n");
      print_errorline();
      handle_instruction_access_fault();
      break;
    case CAUSE_LOAD_ACCESS:
      sprint("handle_mtrap: CAUSE_LOAD_ACCESS\n");
      // panic("Load access fault!");
      print_errorline();
      handle_load_access_fault();
    case CAUSE_STORE_ACCESS:
      sprint("handle_mtrap: CAUSE_STORE_ACCESS\n");
      print_errorline();
      handle_store_access_fault();
      break;
    case CAUSE_ILLEGAL_INSTRUCTION:
      // TODO (lab1_2): call handle_illegal_instruction to implement illegal instruction
      // interception, and finish lab1_2.
//      panic( "call handle_illegal_instruction to accomplish illegal instruction interception for lab1_2.\n" );
      sprint("handle_mtrap: CAUSE_ILLEGAL_INSTRUCTION\n");
      // panic("Illegal instruction!");
      print_errorline();
      handle_illegal_instruction();
      break;
    case CAUSE_MISALIGNED_LOAD:
      sprint("handle_mtrap: CAUSE_MISALIGNED_LOAD\n");
      print_errorline();
      handle_misaligned_load();
      break;
    case CAUSE_MISALIGNED_STORE:
      sprint("handle_mtrap: CAUSE_MISALIGNED_STORE\n");
      print_errorline();
      handle_misaligned_store();
      break;

    default:
      sprint("handle_mtrap: unexpected mscause %p\n", mcause);
      print_errorline();
      sprint("machine trap(): unexpected mscause %p\n", mcause);
      sprint("            mepc=%p mtval=%p\n", read_csr(mepc), read_csr(mtval));
      panic( "unexpected exception happened in M-mode.\n" );
      break;
  }
}
