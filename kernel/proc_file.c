/*
 * Interface functions between file system and kernel/processes. added @lab4_1
 */

#include "proc_file.h"

#include "hostfs.h"
#include "pmm.h"
#include "process.h"
#include "ramdev.h"
#include "rfs.h"
#include "riscv.h"
#include "spike_interface/spike_file.h"
#include "spike_interface/spike_utils.h"
#include "util/functions.h"
#include "util/string.h"
static void transfer_2_absolute_path(char * relative_path,char * absolute_path){
    /**
     * 考虑两种相对路径：'.' - 1 和'..' - 2
     * 通过获取当前进程的cwd，然后逐级向上查找，直到根目录，得到父目录的绝对路径
     * 在和相对路径拼接，得到绝对路径
     */
//     sprint("relative_path:%s\n",relative_path);
    if(relative_path[0] == '/'){
        memcpy(absolute_path,relative_path,strlen(relative_path));
        return;
    }
  
    int type = 0;
    struct dentry * cwd = current->pfiles->cwd;
    // sprint("cwd:%s\n",cwd->name);
    if(relative_path[0] == '.'){
        type = 1;
        if(relative_path[1] == '.'){//相对路径为'../*'，则需要返回上一级目录
            type = 2;
            cwd = cwd->parent;
        }
    }
    for(;cwd;cwd=cwd->parent){
        char tmp[MAX_DEVICE_NAME_LEN];
        memset(tmp,0,MAX_DEVICE_NAME_LEN);
        memcpy(tmp,absolute_path, strlen(absolute_path));
        memset(absolute_path,0,MAX_DEVICE_NAME_LEN);
        memcpy(absolute_path,cwd->name,strlen(cwd->name));
        if(cwd->parent){//如果不是根目录，则需要加上'/'
            absolute_path[strlen(cwd->name)] = '/';
            absolute_path[strlen(cwd->name)+1] = '\0';
        }
        strcat(absolute_path,tmp);
//        sprint("absolute_path:%s\n",absolute_path);
    }
    switch (type) {
        case 1:
            strcat(absolute_path,relative_path+2);//相对路径为'./'，则需要去掉'./'
            break;
        case 2:
            strcat(absolute_path,relative_path+3);//相对路径为'../'，则需要去掉'../'
            break;
        default:
            strcat(absolute_path,relative_path);//路径为'/'，则不需要去掉
            break;
    }
}
//
// initialize file system
//
void fs_init(void) {
  // initialize the vfs
  vfs_init();

  // register hostfs and mount it as the root
  if( register_hostfs() < 0 ) panic( "fs_init: cannot register hostfs.\n" );
  struct device *hostdev = init_host_device("HOSTDEV");
  vfs_mount("HOSTDEV", MOUNT_AS_ROOT);

  // register and mount rfs
  if( register_rfs() < 0 ) panic( "fs_init: cannot register rfs.\n" );
  struct device *ramdisk0 = init_rfs_device("RAMDISK0");
  rfs_format_dev(ramdisk0);
  vfs_mount("RAMDISK0", MOUNT_DEFAULT);
}

//
// initialize a proc_file_management data structure for a process.
// return the pointer to the page containing the data structure.
//
proc_file_management *init_proc_file_management(void) {
  proc_file_management *pfiles = (proc_file_management *)alloc_page();
  pfiles->cwd = vfs_root_dentry; // by default, cwd is the root
  pfiles->nfiles = 0;

  for (int fd = 0; fd < MAX_FILES; ++fd)
    pfiles->opened_files[fd].status = FD_NONE;

  // sprint("FS: created a file management struct for a process.\n");
  return pfiles;
}

//
// reclaim the open-file management data structure of a process.
// note: this function is not used as PKE does not actually reclaim a process.
//
void reclaim_proc_file_management(proc_file_management *pfiles) {
  free_page(pfiles);
  return;
}

//
// get an opened file from proc->opened_file array.
// return: the pointer to the opened file structure.
//
struct file *get_opened_file(int fd) {
  struct file *pfile = NULL;

  // browse opened file list to locate the fd
  for (int i = 0; i < MAX_FILES; ++i) {
    pfile = &(current->pfiles->opened_files[i]);  // file entry
    if (i == fd) break;
  }
  if (pfile == NULL) panic("do_read: invalid fd!\n");
  return pfile;
}

//
// open a file named as "pathname" with the permission of "flags".
// return: -1 on failure; non-zero file-descriptor on success.
//
int do_open(char *pathname, int flags) {
  struct file *opened_file = NULL;
  if ((opened_file = vfs_open(pathname, flags)) == NULL) return -1;

  int fd = 0;
  if (current->pfiles->nfiles >= MAX_FILES) {
    panic("do_open: no file entry for current process!\n");
  }
  struct file *pfile;
  for (fd = 0; fd < MAX_FILES; ++fd) {
    pfile = &(current->pfiles->opened_files[fd]);
    if (pfile->status == FD_NONE) break;
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
int do_read(int fd, char *buf, uint64 count) {
  struct file *pfile = get_opened_file(fd);

  if (pfile->readable == 0) panic("do_read: no readable file!\n");

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
int do_write(int fd, char *buf, uint64 count) {
  struct file *pfile = get_opened_file(fd);

  if (pfile->writable == 0) panic("do_write: cannot write file!\n");

  int len = vfs_write(pfile, buf, count);
  return len;
}

//
// reposition the file offset
//
int do_lseek(int fd, int offset, int whence) {
  struct file *pfile = get_opened_file(fd);
  return vfs_lseek(pfile, offset, whence);
}

//
// read the vinode information
//
int do_stat(int fd, struct istat *istat) {
  struct file *pfile = get_opened_file(fd);
  return vfs_stat(pfile, istat);
}

//
// read the inode information on the disk
//
int do_disk_stat(int fd, struct istat *istat) {
  struct file *pfile = get_opened_file(fd);
  return vfs_disk_stat(pfile, istat);
}

//
// close a file
//
int do_close(int fd) {
  struct file *pfile = get_opened_file(fd);
  return vfs_close(pfile);
}

//
// open a directory
// return: the fd of the directory file
//
int do_opendir(char *pathname) {
  sprint("do_opendir: %s\n", pathname);
  struct file *opened_file = NULL;
  if ((opened_file = vfs_opendir(pathname)) == NULL){
      sprint("open dir failed\n");
      return -1;
  }else{
      sprint("open dir success\n");
  }

  int fd = 0;
  struct file *pfile;
  for (fd = 0; fd < MAX_FILES; ++fd) {
    pfile = &(current->pfiles->opened_files[fd]);
    if (pfile->status == FD_NONE) break;
  }
  if (pfile->status != FD_NONE)  // no free entry
    panic("do_opendir: no file entry for current process!\n");

  // initialize this file structure
  memcpy(pfile, opened_file, sizeof(struct file));

  ++current->pfiles->nfiles;
  return fd;
}

//
// read a directory entry
//
int do_readdir(int fd, struct dir *dir) {
  struct file *pfile = get_opened_file(fd);
  return vfs_readdir(pfile, dir);
}

//
// make a new directory
//
int do_mkdir(char *pathname) {
  return vfs_mkdir(pathname);
}

//
// close a directory
//
int do_closedir(int fd) {
  struct file *pfile = get_opened_file(fd);
  return vfs_closedir(pfile);
}

//
// create hard link to a file
//
int do_link(char *oldpath, char *newpath) {
  return vfs_link(oldpath, newpath);
}

//
// remove a hard link to a file
//
int do_unlink(char *path) {
  return vfs_unlink(path);
}

int do_read_cwd(char *pathpa) {
    memset(pathpa, '\0', MAX_DEVICE_NAME_LEN);
    struct dentry *cwd = current->pfiles->cwd;
    if(cwd == NULL) panic("read cwd failed! cwd is NULL\n");
    else if(cwd->parent==NULL){
        // sprint("do_read_cwd: cwd is root\n");
        strcpy(pathpa,"/");
        return 0;
    }else {
        char absolute_path[MAX_DEVICE_NAME_LEN];
        memset(absolute_path, '\0', MAX_DEVICE_NAME_LEN);
        transfer_2_absolute_path(pathpa, absolute_path);
        // sprint("do_read_cwd: absolute_path:%s\n", absolute_path);
        strcpy(pathpa, absolute_path);
        pathpa[strlen(pathpa) - 1] = '\0';//TODO
    }

    return 0;
}

int do_change_cwd(char *pathpa) {
    char absolute_path[MAX_DEVICE_NAME_LEN];
    memset(absolute_path,0,MAX_DEVICE_NAME_LEN);
    transfer_2_absolute_path(pathpa,absolute_path);
    int found = do_opendir(absolute_path);
    current->pfiles->cwd = current->pfiles->opened_files[found].f_dentry;//实现change cwd
    sprint("do_change_cwd:current->pfiles->cwd:%s\n",current->pfiles->cwd->name);
    do_closedir(found);

    return 0;
}