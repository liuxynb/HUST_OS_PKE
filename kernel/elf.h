#ifndef _ELF_H_
#define _ELF_H_

#include "util/types.h"
#include "process.h"

#define MAX_CMDLINE_ARGS 64
#define ELF_MAGIC 0x464C457FU // "\x7FELF" in little endian
#define ELF_PROG_LOAD 1
#define ELF_SYMTAB 2 // symbol table
#define ELF_STRTAB 3 // string table
#define STT_FILE 4   // symbol name is file name
#define STT_FUNC 18  // symbol name is function name

// elf header structure
typedef struct elf_header_t
{
  uint32 magic;
  uint8 elf[12];
  uint16 type;      /* Object file type */
  uint16 machine;   /* Architecture */
  uint32 version;   /* Object file version */
  uint64 entry;     /* Entry point virtual address */
  uint64 phoff;     /* Program header table file offset */
  uint64 shoff;     /* Section header table file offset */
  uint32 flags;     /* Processor-specific flags */
  uint16 ehsize;    /* ELF header size in bytes */
  uint16 phentsize; /* Program header table entry size */
  uint16 phnum;     /* Program header table entry count */
  uint16 shentsize; /* Section header table entry size */
  uint16 shnum;     /* Section header table entry count */
  uint16 shstrndx;  /* Section header string table index */
} elf_header;

// Program segment header.
typedef struct elf_prog_header_t
{
  uint32 type;   /* Segment type */
  uint32 flags;  /* Segment flags */
  uint64 off;    /* Segment file offset */
  uint64 vaddr;  /* Segment virtual address */
  uint64 paddr;  /* Segment physical address */
  uint64 filesz; /* Segment size in file */
  uint64 memsz;  /* Segment size in memory */
  uint64 align;  /* Segment alignment */
} elf_prog_header;

typedef enum elf_status_t
{
  EL_OK = 0,

  EL_EIO,    // I/O error
  EL_ENOMEM, // Out of memory
  EL_NOTELF, // Not an ELF file
  EL_ERR,

} elf_status;

// elf section header structure
// ! reference: https://man7.org/linux/man-pages/man5/elf.5.html
typedef struct elf_section_header_t
{
  uint32 sh_name;      /* Section name */
  uint32 sh_type;      /* Section type */
  uint64 sh_flags;     /* Section flags */
  uint64 sh_addr;      /* Section virtual address at execution */
  uint64 sh_offset;    /* Section offset of file */
  uint64 sh_size;      /* Section's size in bytes */
  uint32 sh_link;      /* Section header table link which links to another section */
  uint32 sh_info;      /* Extra information */
  uint64 sh_addralign; /* Section address alignment constrainents */
  uint64 sh_entsize;   /* Entry size in bytes if section holds table */
} elf_section_header;

// ! reference: https://man7.org/linux/man-pages/man5/elf.5.html
// string and symbol tables structure
typedef struct elf_symbol_t
{
  uint32 st_name;         /* Symbol names if nonzero */
  unsigned char st_info;  /* Symbol's type and binding attributes */
  unsigned char st_other; /* Symbol visibility */
  uint16 st_shndx;        /* Symbol section header table index */
  uint64 st_value;        /* Symbol value */
  uint64 st_size;         /* Symbol size */
} elf_symbol;

typedef struct elf_ctx_t
{
  void *info;
  elf_header ehdr;
  // added on lab3_challenge1
  char str_table[4096];   // string table, used to store the name of each section
  elf_symbol symbol[128]; // symbol table, used to store the symbol information
  uint64 symbol_num;
} elf_ctx; // elf context

elf_status elf_init(elf_ctx *ctx, void *info);
elf_status elf_load(elf_ctx *ctx);
elf_status elf_load_symbol(elf_ctx *ctx);
elf_status elf_print_name(uint64 ra, int *current_depth, int depth);
void load_bincode_from_host_elf(process *p);
#endif
