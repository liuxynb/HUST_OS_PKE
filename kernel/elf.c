/*
 * routines that scan and load a (host) Executable and Linkable Format (ELF) file
 * into the (emulated) memory.
 */

#include "elf.h"
#include "string.h"
#include "riscv.h"
#include "vmm.h"
#include "pmm.h"
#include "vfs.h"
#include "spike_interface/spike_utils.h"

typedef struct elf_info_t {
  struct file *f;
  process *p;
} elf_info;


struct elf_sym_table elf_sym_tab;
//
// the implementation of allocater. allocates memory space for later segment loading.
// this allocater is heavily modified @lab2_1, where we do NOT work in bare mode.
//
static void *elf_alloc_mb(elf_ctx *ctx, uint64 elf_pa, uint64 elf_va, uint64 size) {
  elf_info *msg = (elf_info *)ctx->info;
  // we assume that size of proram segment is smaller than a page.
  // sprint("*****size: %lx*****\n", size);
  kassert(size < PGSIZE);
  void *pa = alloc_page();
  if (pa == 0) panic("uvmalloc mem alloc falied\n");

  memset((void *)pa, 0, PGSIZE);
  user_vm_map((pagetable_t)msg->p->pagetable, elf_va, PGSIZE, (uint64)pa,
         prot_to_type(PROT_WRITE | PROT_READ | PROT_EXEC, 1));

  return pa;
}
static void *elf_process_alloc_mb(process *p, uint64 elf_pa, uint64 elf_va, uint64 size) {
  // we assume that size of proram segment is smaller than a page.
  kassert(size < PGSIZE);
  void *pa = alloc_page();
  if (pa == 0) panic("uvmalloc mem alloc falied\n");

  // memset((void *)pa, 0, PGSIZE);
  user_vm_map(p->pagetable, elf_va, PGSIZE, (uint64)pa,
         prot_to_type(PROT_WRITE | PROT_READ | PROT_EXEC, 1));
  return pa;
}


// leb128 (little-endian base 128) is a variable-length
// compression algoritm in DWARF
void read_uleb128(uint64 *out, char **off) {
    uint64 value = 0; int shift = 0; uint8 b;
    for (;;) {
        b = *(uint8 *)(*off); (*off)++;
        value |= ((uint64)b & 0x7F) << shift;
        shift += 7;
        if ((b & 0x80) == 0) break;
    }
    if (out) *out = value;
}
void read_sleb128(int64 *out, char **off) {
    int64 value = 0; int shift = 0; uint8 b;
    for (;;) {
        b = *(uint8 *)(*off); (*off)++;
        value |= ((uint64_t)b & 0x7F) << shift;
        shift += 7;
        if ((b & 0x80) == 0) break;
    }
    if (shift < 64 && (b & 0x40)) value |= -(1 << shift);
    if (out) *out = value;
}

// Since reading below types through pointer cast requires aligned address,
// so we can only read them byte by byte
void read_uint64(uint64 *out, char **off) {
    *out = 0;
    for (int i = 0; i < 8; i++) {
        *out |= (uint64)(**off) << (i << 3); (*off)++;
    }
}
void read_uint32(uint32 *out, char **off) {
    *out = 0;
    for (int i = 0; i < 4; i++) {
        *out |= (uint32)(**off) << (i << 3); (*off)++;
    }
}
void read_uint16(uint16 *out, char **off) {
    *out = 0;
    for (int i = 0; i < 2; i++) {
        *out |= (uint16)(**off) << (i << 3); (*off)++;
    }
}

//
// actual file reading, using the vfs file interface.
//
static uint64 elf_fpread(elf_ctx *ctx, void *dest, uint64 nb, uint64 offset) {
  elf_info *msg = (elf_info *)ctx->info;
  vfs_lseek(msg->f, offset, SEEK_SET);
  return vfs_read(msg->f, dest, nb);
}
//
// init elf_ctx, a data structure that loads the elf.
//
elf_status elf_init(elf_ctx *ctx, void *info) {
  ctx->info = info;

  // load the elf header
  if (elf_fpread(ctx, &ctx->ehdr, sizeof(ctx->ehdr), 0) != sizeof(ctx->ehdr)) return EL_EIO;

  // check the signature (magic value) of the elf
  if (ctx->ehdr.magic != ELF_MAGIC) return EL_NOTELF;

  return EL_OK;
}


/*
* analyzis the data in the debug_line section
*
* the function needs 3 parameters: elf context, data in the debug_line section
* and length of debug_line section
*
* make 3 arrays:
* "process->dir" stores all directory paths of code files
* "process->file" stores all code file names of code files and their directory path index of array "dir"
* "process->line" stores all relationships map instruction addresses to code line numbers
* and their code file name index of array "file"
*/
/*
 * 修改ctx中的info中的process的debug_line(char *)，dir(char **), file(code_file *), line(addr_line *)
 * dir,file,line都是数组，存储在debug_line缓冲区中
 * dir: 存储代码文件的目录名
 * file: 存储代码文件名和目录名的索引
 * line: 存储指令地址、代码行号和代码文件名在file数组中的索引
 * eg. 如某文件第3行为a = 0，被编译成地址为0x1234处的汇编代码li ax, 0和0x1238处的汇编代码sd 0(s0), ax。
 * 那么file数组中就包含两项，addr属性分别为0x1234和0x1238，line属性为3，file属性为“某文件”的文件名在file数组中的索引。
 * 通过解析debug_line中的数据，填充dir、file、line数组
 */
void make_addr_line(elf_ctx *ctx, char *debug_line, uint64 length)
{
  process *p = ((elf_info *)ctx->info)->p;
  // directory name char pointer array
  p->dir = (char **)((((uint64)debug_line + length + 7) >> 3) << 3);
  memset(p->dir, 0, 64 * sizeof(char *));

  int dir_ind = 0, dir_base;
  // file name char pointer array
  p->file = (code_file *)(p->dir + 64);
  memset(p->file, 0, 64 * sizeof(code_file));
  int file_ind = 0, file_base;
  // table array
  p->line = (addr_line *)(p->file + 64);
  memset(p->line, 0, 64 * sizeof(addr_line));
  p->line_ind = 0;
  char *off = debug_line;
  while (off < debug_line + length)
  { // iterate each compilation unit(CU)
    debug_header *dh = (debug_header *)off;
    off += sizeof(debug_header); // += 27
    dir_base = dir_ind;
    file_base = file_ind;
    // get directory name char pointer in this CU
    while (*off != 0)
    {
      p->dir[dir_ind++] = off;
      while (*off != 0)
        off++;
      off++;
    }
    off++;
    // get file name char pointer in this CU
    while (*off != 0)
    {
      p->file[file_ind].file = off;
      while (*off != 0)
        off++;
      off++;
      uint64 dir;
      read_uleb128(&dir, &off);
      p->file[file_ind++].dir = dir - 1 + dir_base;
      // sprint("file:%s\n", p->file[file_ind - 1].file);
      read_uleb128(NULL, &off);
      read_uleb128(NULL, &off);
    }
    off++;
    addr_line regs;
    regs.addr = 0;
    regs.file = 1;
    regs.line = 1;
    // simulate the state machine op code
    for (;;)
    {
      uint8 op = *(off++);
      switch (op)
      {
      case 0: // Extended Opcodes
        read_uleb128(NULL, &off);
        op = *(off++);
        switch (op)
        {
        case 1: // DW_LNE_end_sequence
          if (p->line_ind > 0 && p->line[p->line_ind - 1].addr == regs.addr)
            p->line_ind--;
          p->line[p->line_ind] = regs;
          p->line[p->line_ind].file += file_base - 1;
          p->line_ind++;
          goto endop;
        case 2: // DW_LNE_set_address
          read_uint64(&regs.addr, &off);
          break;
        // ignore DW_LNE_define_file
        case 4: // DW_LNE_set_discriminator
          read_uleb128(NULL, &off);
          break;
        }
        break;
      case 1: // DW_LNS_copy
        if (p->line_ind > 0 && p->line[p->line_ind - 1].addr == regs.addr)
          p->line_ind--;
        p->line[p->line_ind] = regs;
        p->line[p->line_ind].file += file_base - 1;
        p->line_ind++;
        break;
      case 2:
      { // DW_LNS_advance_pc
        uint64 delta;
        read_uleb128(&delta, &off);
        regs.addr += delta * dh->min_instruction_length;
        break;
      }
      case 3:
      { // DW_LNS_advance_line
        int64 delta;
        read_sleb128(&delta, &off);
        regs.line += delta;
        break;
      }
      case 4: // DW_LNS_set_file
        read_uleb128(&regs.file, &off);
        break;
      case 5: // DW_LNS_set_column
        read_uleb128(NULL, &off);
        break;
      case 6: // DW_LNS_negate_stmt
      case 7: // DW_LNS_set_basic_block
        break;
      case 8:
      { // DW_LNS_const_add_pc
        int adjust = 255 - dh->opcode_base;
        int delta = (adjust / dh->line_range) * dh->min_instruction_length;
        regs.addr += delta;
        break;
      }
      case 9:
      { // DW_LNS_fixed_advanced_pc
        uint16 delta;
        read_uint16(&delta, &off);
        regs.addr += delta;
        break;
      }
        // ignore 10, 11 and 12
      default:
      { // Special Opcodes
        int adjust = op - dh->opcode_base;
        int addr_delta = (adjust / dh->line_range) * dh->min_instruction_length;
        int line_delta = dh->line_base + (adjust % dh->line_range);
        regs.addr += addr_delta;
        regs.line += line_delta;
        if (p->line_ind > 0 && p->line[p->line_ind - 1].addr == regs.addr)
          p->line_ind--;
        p->line[p->line_ind] = regs;
        p->line[p->line_ind].file += file_base - 1;
        p->line_ind++;
        break;
      }
      }
    }
  endop:;
  }
  // for (int i = 0; i < p->line_ind; i++)
  //     sprint("%p %d %d\n", p->line[i].addr, p->line[i].line, p->line[i].file);
  // sprint("%lx %lx\n", p->dir[0], p->dir[1]);
}

//
// load the elf segments to memory regions.
//
elf_status elf_load(elf_ctx *ctx) {
  // elf_prog_header structure is defined in kernel/elf.h
  elf_prog_header ph_addr;
  int i, off;

  // traverse the elf program segment headers
  for (i = 0, off = ctx->ehdr.phoff; i < ctx->ehdr.phnum; i++, off += sizeof(ph_addr)) {
    // read segment headers
    if (elf_fpread(ctx, (void *)&ph_addr, sizeof(ph_addr), off) != sizeof(ph_addr)) return EL_EIO;

    if (ph_addr.type != ELF_PROG_LOAD) continue;
    if (ph_addr.memsz < ph_addr.filesz) return EL_ERR;
    if (ph_addr.vaddr + ph_addr.memsz < ph_addr.vaddr) return EL_ERR;

    // allocate memory block before elf loading
    void *dest = elf_alloc_mb(ctx, ph_addr.vaddr, ph_addr.vaddr, ph_addr.memsz);

    // actual loading
    if (elf_fpread(ctx, dest, ph_addr.memsz, ph_addr.off) != ph_addr.memsz)
      return EL_EIO;

    // record the vm region in proc->mapped_info. added @lab3_1
    int j;
    for( j=0; j<PGSIZE/sizeof(mapped_region); j++ ) //seek the last mapped region
      if( (process*)(((elf_info*)(ctx->info))->p)->mapped_info[j].va == 0x0 ) break;

    ((process*)(((elf_info*)(ctx->info))->p))->mapped_info[j].va = ph_addr.vaddr;
    ((process*)(((elf_info*)(ctx->info))->p))->mapped_info[j].npages = 1;

    // SEGMENT_READABLE, SEGMENT_EXECUTABLE, SEGMENT_WRITABLE are defined in kernel/elf.h
    if( ph_addr.flags == (SEGMENT_READABLE|SEGMENT_EXECUTABLE) ){
      ((process*)(((elf_info*)(ctx->info))->p))->mapped_info[j].seg_type = CODE_SEGMENT;
      // sprint( "CODE_SEGMENT added at mapped info offset:%d\n", j );
    }else if ( ph_addr.flags == (SEGMENT_READABLE|SEGMENT_WRITABLE) ){
      ((process*)(((elf_info*)(ctx->info))->p))->mapped_info[j].seg_type = DATA_SEGMENT;
      // sprint( "DATA_SEGMENT added at mapped info offset:%d\n", j );
    }else
      panic( "unknown program segment encountered, segment flag:%d.\n", ph_addr.flags );

    ((process*)(((elf_info*)(ctx->info))->p))->total_mapped_region ++;
  }
  /***** Part2 遍历所有的section header，找到.debug_line section，并进行解析 ****/
  elf_sect_header shstrtab,tmp;

  //读入shstrtab

  if(elf_fpread(ctx,(void*)&shstrtab,sizeof(shstrtab),
                ctx->ehdr.shoff + ctx->ehdr.shstrndx * sizeof(shstrtab)) != sizeof(shstrtab))
      return EL_EIO;
  for(i=0,off=ctx->ehdr.shoff;i < ctx->ehdr.shnum;i++,off+=sizeof(tmp)){
      //读入section header_i
    if(elf_fpread(ctx,(void*)&tmp,sizeof(tmp),off) != sizeof(tmp))
        return EL_EIO;
    char all_name[shstrtab.size];
    //读入shstrtab中的所有字符串
    elf_fpread(ctx,&all_name,shstrtab.size,shstrtab.offset);

    //判断是否为.debug_line section
    if(strcmp(".debug_line",all_name+tmp.name) == 0){

      if(elf_fpread(ctx, ((process*)(((elf_info*)(ctx->info))->p))->debugline,tmp.size,tmp.offset) != tmp.size){
          return EL_EIO;
      }
      else{
        // sprint("debug_line section loaded, size:%d\n", tmp.size);
        // sprint("debug_line==%s\n", ((process*)(((elf_info*)(ctx->info))->p))->debugline);
      }
        make_addr_line(ctx, ((process*)(((elf_info*)(ctx->info))->p))->debugline,tmp.size);
        break;
    }
  }

  return EL_OK;
}


void load_sym_tab(elf_ctx * elf_ctx){
    elf_sect_header shstrtab;
    elf_sect_header symtab;
    elf_sect_header strtab;
    elf_sect_header tmp;


    //get the shstrtab
    uint16 shnum = elf_ctx->ehdr.shnum;
    uint64 str_off = elf_ctx->ehdr.shoff + elf_ctx->ehdr.shstrndx * sizeof(elf_sect_header) ;
    elf_fpread(elf_ctx,(void * )&shstrtab,sizeof (shstrtab),str_off);
    char tmpstr[shstrtab.size];
    uint64 shstrtab_off = shstrtab.offset;
    elf_fpread(elf_ctx,&tmpstr,shstrtab.size,shstrtab_off);
//    sprint("tmp_str:%s\n",tmpstr);
    //get the symtab and strtab
//    sprint("off:%d\n",elf_ctx->ehdr.shoff);
    for (int i = 0, off = elf_ctx->ehdr.shoff; i < shnum; i++, off += sizeof(elf_sect_header)) {
        //遍历节头表，通过type和name找到.symtab和.strtab
        elf_fpread(elf_ctx,(void * )&tmp,sizeof (tmp),off);//读取section header
        if (tmp.type == 2 && strcmp(tmpstr+tmp.name,".symtab")==0) {//找到.symtab
            symtab = tmp;
        }else if (tmp.type == 3 && strcmp(tmpstr+tmp.name,".strtab")==0) {//找到.strtab
            strtab = tmp;
        }
    }

    //collect all the function in symtab
    int sym_count = symtab.size / sizeof(elf_sym);
    elf_sym_tab.sym_count = 0;
    for(int i = 0; i < sym_count; i++){
        elf_sym sym;
        elf_fpread(elf_ctx,(void * )&sym,sizeof (sym),symtab.offset + i * sizeof(elf_sym));
        if (sym.st_name == 0) {
            continue;
        }
        if(sym.st_info == 0x12){
            char ntmp[32];
            elf_fpread(elf_ctx,(void * )&ntmp,sizeof (ntmp),strtab.offset + sym.st_name);
            strcpy(elf_sym_tab.sym_names[elf_sym_tab.sym_count],ntmp);
            elf_sym_tab.sym[elf_sym_tab.sym_count] = sym;
            elf_sym_tab.sym_count++;
        }
    }
//    for(int i = 0; i < 2; i++){
//        sprint("sym:%s\n",elf_sym_tab.sym_names[i]);
//    }
}

//
// load the elf of user application, by using the spike file interface.
//
void load_bincode_from_host_elf(process *p,char* filename) {


  sprint("Application: %s\n", filename);

  //elf loading. elf_ctx is defined in kernel/elf.h, used to track the loading process.
  elf_ctx elfloader;
  // elf_info is defined above, used to tie the elf file and its corresponding process.
  elf_info info;

  info.f = vfs_open(filename, O_RDONLY);
  info.p = p;
  // IS_ERR_VALUE is a macro defined in spike_interface/spike_htif.h
  if (IS_ERR_VALUE(info.f)) panic("Fail on openning the input application program.\n");

  // init elfloader context. elf_init() is defined above.
  if (elf_init(&elfloader, &info) != EL_OK)
    panic("fail to init elfloader.\n");

  // load elf. elf_load() is defined above.
  if (elf_load(&elfloader) != EL_OK) panic("Fail on loading elf.\n");


  load_sym_tab(&elfloader);
  // entry (virtual, also physical in lab1_x) address
  p->trapframe->epc = elfloader.ehdr.entry;

  // close the host spike file
  vfs_close( info.f );

  sprint("Application program entry point (virtual address): 0x%lx\n", p->trapframe->epc);
}

