/*
 * Interface functions between file system and kernel/processes. added @lab4_1
 */

#include "proc_file.h"

#include "hostfs.h"
#include "pmm.h"
#include "vmm.h"
#include "process.h"
#include "ramdev.h"
#include "rfs.h"
#include "riscv.h"
#include "spike_interface/spike_file.h"
#include "spike_interface/spike_utils.h"
#include "util/functions.h"
#include "util/string.h"

//
// initialize file system
//
void fs_init(void)
{
  // initialize the vfs
  vfs_init();

  // register hostfs and mount it as the root
  if (register_hostfs() < 0)
    panic("fs_init: cannot register hostfs.\n");
  struct device *hostdev = init_host_device("HOSTDEV");
  vfs_mount("HOSTDEV", MOUNT_AS_ROOT);

  // register and mount rfs
  if (register_rfs() < 0)
    panic("fs_init: cannot register rfs.\n");
  struct device *ramdisk0 = init_rfs_device("RAMDISK0");
  rfs_format_dev(ramdisk0);
  vfs_mount("RAMDISK0", MOUNT_DEFAULT);
}

//
// initialize a proc_file_management data structure for a process.
// return the pointer to the page containing the data structure.
//
proc_file_management *init_proc_file_management(void)
{
  proc_file_management *pfiles = (proc_file_management *)alloc_page();
  pfiles->cwd = vfs_root_dentry; // by default, cwd is the root
  pfiles->nfiles = 0;

  for (int fd = 0; fd < MAX_FILES; ++fd)
    pfiles->opened_files[fd].status = FD_NONE;

  sprint("FS: created a file management struct for a process.\n");
  return pfiles;
}

//
// reclaim the open-file management data structure of a process.
// note: this function is not used as PKE does not actually reclaim a process.
//
void reclaim_proc_file_management(proc_file_management *pfiles)
{
  free_page(pfiles);
  return;
}

//
// get an opened file from proc->opened_file array.
// return: the pointer to the opened file structure.
//
struct file *get_opened_file(int fd)
{
  struct file *pfile = NULL;

  // browse opened file list to locate the fd
  for (int i = 0; i < MAX_FILES; ++i)
  {
    pfile = &(current->pfiles->opened_files[i]); // file entry
    if (i == fd)
      break;
  }
  if (pfile == NULL)
    panic("do_read: invalid fd!\n");
  return pfile;
}

//
// open a file named as "pathname" with the permission of "flags".
// return: -1 on failure; non-zero file-descriptor on success.
//
int do_open(char *path, int flags)
{
  // do parse on path
  char paresd_path[256];

  if (path[0] == '.' && path[1] == '.') // 据文档的意思扩充解析路径的功能，支持相对路径
  {
    path += 2;
    if (current->pfiles->cwd->parent != NULL)
    {
      strcpy(paresd_path, current->pfiles->cwd->parent->name);
      if (!strcmp(paresd_path, "/"))
        *paresd_path = 0;
      strcat(paresd_path, path);
    }
    else
      strcpy(paresd_path, "/");
  }
  else if (path[0] == '.' && path[1] != '.')
  {
    path++;
    strcpy(paresd_path, current->pfiles->cwd->name);
    if (!strcmp(paresd_path, "/"))
      *paresd_path = 0;
    strcat(paresd_path, path); // 将path指向的字符串拼接到当前目录的路径后面
  }
  else
  {
    strcpy(paresd_path, path);
  }
  struct file *opened_file = NULL;
  // sprint("do_open: %s\n", paresd_path);
  if ((opened_file = vfs_open(paresd_path, flags)) == NULL)
    return -1;

  int fd = 0;
  if (current->pfiles->nfiles >= MAX_FILES)
  {
    panic("do_open: no file entry for current process!\n");
  }
  struct file *pfile;
  for (fd = 0; fd < MAX_FILES; ++fd)
  {
    pfile = &(current->pfiles->opened_files[fd]);
    if (pfile->status == FD_NONE)
      break;
  }

  // initialize this file structure
  memcpy(pfile, opened_file, sizeof(struct file));

  ++current->pfiles->nfiles;
  return fd;
}

//
// read content of a file ("fd") into "buf" for "count".
// return: actual length of data read from the file.
//
int do_read(int fd, char *buf, uint64 count)
{
  struct file *pfile = get_opened_file(fd);

  if (pfile->readable == 0)
    panic("do_read: no readable file!\n");

  char buffer[count + 1];
  int len = vfs_read(pfile, buffer, count);
  buffer[count] = '\0';
  strcpy(buf, buffer);
  return len;
}

//
// write content ("buf") whose length is "count" to a file "fd".
// return: actual length of data written to the file.
//
int do_write(int fd, char *buf, uint64 count)
{
  struct file *pfile = get_opened_file(fd);

  if (pfile->writable == 0)
    panic("do_write: cannot write file!\n");

  int len = vfs_write(pfile, buf, count);
  return len;
}

//
// reposition the file offset
//
int do_lseek(int fd, int offset, int whence)
{
  struct file *pfile = get_opened_file(fd);
  return vfs_lseek(pfile, offset, whence);
}

//
// read the vinode information
//
int do_stat(int fd, struct istat *istat)
{
  struct file *pfile = get_opened_file(fd);
  return vfs_stat(pfile, istat);
}

//
// read the inode information on the disk
//
int do_disk_stat(int fd, struct istat *istat)
{
  struct file *pfile = get_opened_file(fd);
  return vfs_disk_stat(pfile, istat);
}

//
// close a file
//
int do_close(int fd)
{
  struct file *pfile = get_opened_file(fd);
  return vfs_close(pfile);
}

//
// open a directory
// return: the fd of the directory file
//
int do_opendir(char *pathname)
{
  struct file *opened_file = NULL;
  if ((opened_file = vfs_opendir(pathname)) == NULL)
    return -1;

  int fd = 0;
  struct file *pfile;
  for (fd = 0; fd < MAX_FILES; ++fd)
  {
    pfile = &(current->pfiles->opened_files[fd]);
    if (pfile->status == FD_NONE)
      break;
  }
  if (pfile->status != FD_NONE) // no free entry
    panic("do_opendir: no file entry for current process!\n");

  // initialize this file structure
  memcpy(pfile, opened_file, sizeof(struct file));

  ++current->pfiles->nfiles;
  return fd;
}

//
// read a directory entry
//
int do_readdir(int fd, struct dir *dir)
{
  struct file *pfile = get_opened_file(fd);
  return vfs_readdir(pfile, dir);
}

//
// make a new directory
//
int do_mkdir(char *pathname)
{
  return vfs_mkdir(pathname);
}

//
// close a directory
//
int do_closedir(int fd)
{
  struct file *pfile = get_opened_file(fd);
  return vfs_closedir(pfile);
}

//
// create hard link to a file
//
int do_link(char *oldpath, char *newpath)
{
  return vfs_link(oldpath, newpath);
}

//
// remove a hard link to a file
//
int do_unlink(char *path)
{
  return vfs_unlink(path);
}

//
// read present working directory (pwd)
//
int do_rewd(char *path)
{
  if (current->pfiles->cwd == NULL)
    return -1;
  strcpy((char *)user_va_to_pa((pagetable_t)current->pagetable, path), current->pfiles->cwd->name); // 将当前目录的路径复制到path中
  return 0;
}

//
// change pwd
// 从路径字符串形式的角度来看，相对路径的起始处总是为以下两种特殊的目录之一：”.“，”..“。
// 其中”.“代指进程的当前工作目录，“..”代指进程当前工作目录的父目录。
// 例如，“./file”的含义为：位于进程当前工作目录下的file文件；
// “../dir/file”的含义为：当前进程工作目录父目录下的dir目录下的file文件。
//
int do_ccwd(char *path)
{
  // panic("do_ccwd: not implemented!\n");
  if (current->pfiles->cwd == NULL)
    return -1;
  char *pathpa = (char *)user_va_to_pa((pagetable_t)current->pagetable, path); // 将path的虚拟地址转换为物理地址.这里pa应该不会不存在
  if (pathpa[0] == '.' && pathpa[1] == '.')
  {
    if (current->pfiles->cwd->parent != NULL)
    {
      // 首先退回到父目录
      strcpy(current->pfiles->cwd->name, current->pfiles->cwd->parent->name); // 将当前目录的父目录的路径复制到当前目录的路径中
      struct dentry *p = current->pfiles->cwd->parent;
      current->pfiles->cwd->parent->parent = current->pfiles->cwd;
      free_page(p);
      pathpa += 2;
      if (pathpa[0] == '/')
        pathpa++;
      strcat(current->pfiles->cwd->name, pathpa); // 将pa指向的字符串拼接到当前目录的路径后面
    }
    else
      strcpy(current->pfiles->cwd->name, "/");
  }
  else if (pathpa[0] == '.' && pathpa[1] != '.')
  {
    pathpa++;
    if (!strcmp(current->pfiles->cwd->name, "/"))
      *current->pfiles->cwd->name = 0;
    strcat(current->pfiles->cwd->name, pathpa);
  }
  else
  {
    strcpy(current->pfiles->cwd->name, pathpa);
  }
  return 0;
}
