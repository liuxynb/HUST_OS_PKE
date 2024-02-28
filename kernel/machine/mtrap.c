#include "kernel/riscv.h"
#include "kernel/process.h"
#include "string.h"
#include "spike_interface/spike_utils.h"


static void print_errorline();

static void handle_instruction_access_fault() { panic("Instruction access fault!"); }

static void handle_load_access_fault() { panic("Load access fault!"); }

static void handle_store_access_fault() { panic("Store/AMO access fault!"); }

static void handle_illegal_instruction() { print_errorline(); panic("Illegal instruction!"); }

static void handle_misaligned_load() { panic("Misaligned Load!"); } // added @lab1_3

static void handle_misaligned_store() { panic("Misaligned AMO!"); }

// added @lab1_3
static void handle_timer()
{
  int cpuid = 0;
  // setup the timer fired at next time (TIMER_INTERVAL from now)
  *(uint64 *)CLINT_MTIMECMP(cpuid) = *(uint64 *)CLINT_MTIMECMP(cpuid) + TIMER_INTERVAL;

  // setup a soft interrupt in sip (S-mode Interrupt Pending) to be handled in S-mode
  write_csr(sip, SIP_SSIP);
}

// added @lab1_challenge2
char error_code_line[1024];

static void read_errorline(struct stat*file_stat, char *error_path)
{
  // read the error code from the file
  spike_file_t *file = spike_file_open(error_path, O_RDONLY, 0);
  // sprint("file: %p\n", file);
  spike_file_stat(file, file_stat);
  spike_file_read(file, error_code_line, (*file_stat).st_size); // read the whole file
  spike_file_close(file);
  // sprint("file size: %d\n", file_stat.st_size);
}

static void print_errorline()
{
  uint64 epc = read_csr(mepc); // the address of the instruction that caused the error
  char error_path[128]; // the path of the error file
  
  struct stat file_stat;
  for(int i=0; i<current->line_ind; i++)
  {
    if(current->line[i].addr > epc) 
    {
      //current->line[i-1] is the line that contains the error
      int length = strlen(current->dir[current->file[current->line[i-1].file].dir]); 
      strcpy(error_path, current->dir[current->file[current->line[i-1].file].dir]);
      error_path[strlen(error_path)] = '/';
      strcpy(error_path+strlen(error_path), current->file[current->line[i-1].file].file);
      // sprint("error path: %s\n", error_path);
      read_errorline(&file_stat, error_path);
      // print the error line
      for(int offset = 0, j = 0; offset < file_stat.st_size; j++)
      {
        int end_of_line = offset; 
        while(error_code_line[end_of_line] != '\n' && end_of_line < file_stat.st_size) end_of_line++;// find the end of the line
        if(j == (current->line[i-1].line-1))
        {
          error_code_line[end_of_line] = '\0';
          sprint("Runtime error at %s:%d\n%s\n", error_path, current->line[i-1].line, error_code_line+offset);
          return;
        }
        offset = end_of_line + 1;
      }
      panic("error line not found\n");
    }
  }
  

}

//
// handle_mtrap calls a handling function according to the type of a machine mode interrupt (trap).
//
void handle_mtrap()
{
  uint64 mcause = read_csr(mcause);
  switch (mcause)
  {
  case CAUSE_MTIMER:
    handle_timer();
    break;
  case CAUSE_FETCH_ACCESS:
    handle_instruction_access_fault();
    break;
  case CAUSE_LOAD_ACCESS:
    handle_load_access_fault();
  case CAUSE_STORE_ACCESS:
    handle_store_access_fault();
    break;
  case CAUSE_ILLEGAL_INSTRUCTION:
    // TODO (lab1_2): call handle_illegal_instruction to implement illegal instruction
    // interception, and finish lab1_2.
    // panic( "call handle_illegal_instruction to accomplish illegal instruction interception for lab1_2.\n" );
    handle_illegal_instruction();
    break;
  case CAUSE_MISALIGNED_LOAD:
    handle_misaligned_load();
    break;
  case CAUSE_MISALIGNED_STORE:
    handle_misaligned_store();
    break;

  default:
    sprint("machine trap(): unexpected mscause %p\n", mcause);
    sprint("            mepc=%p mtval=%p\n", read_csr(mepc), read_csr(mtval));
    panic("unexpected exception happened in M-mode.\n");
    break;
  }
}
