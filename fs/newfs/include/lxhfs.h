#ifndef _LXHFS_H_
#define _LXHFS_H_

#define FUSE_USE_VERSION 26
#include "stdio.h"
#include "stdlib.h"
#include <unistd.h>
#include "fcntl.h"
#include "string.h"
#include "fuse.h"
#include <stddef.h>
#include "ddriver.h"
#include "errno.h"
#include "types.h"

#define LXHFS_MAGIC           0x20020510       /* TODO: Define by yourself */
#define LXHFS_DEFAULT_PERM    0777   /* 全权限打开 */
/******************************************************************************
* SECTION: macro debug
*******************************************************************************/
#define LXHFS_DBG(fmt, ...) do { printf("LXHFS_DBG: " fmt, ##__VA_ARGS__); } while(0) 
/******************************************************************************
* SECTION: lxhfs_utils.c
*******************************************************************************/
char* 				 lxhfs_get_fname(const char* path);
int 			     lxhfs_calc_lvl(const char * path);
int 			     lxhfs_driver_read(int offset, uint8_t *out_content, int size);
int 			     lxhfs_driver_write(int offset, uint8_t *in_content, int size);
int 			     lxhfs_alloc_dentry(struct lxhfs_inode* inode, struct lxhfs_dentry* dentry);
struct lxhfs_inode*  lxhfs_alloc_inode(struct lxhfs_dentry * dentry);
int 				 lxhfs_sync_inode(struct lxhfs_inode * inode);
struct lxhfs_inode*  lxhfs_read_inode(struct lxhfs_dentry * dentry, int ino);
struct lxhfs_dentry* lxhfs_get_dentry(struct lxhfs_inode * inode, int dir);
struct lxhfs_dentry* lxhfs_lookup(const char * path, boolean* is_find, boolean* is_root);
int 				 lxhfs_mount(struct custom_options options);
int 				 lxhfs_umount();
/******************************************************************************
* SECTION: lxhfs.c
*******************************************************************************/
void* 			   lxhfs_init(struct fuse_conn_info *);
void  			   lxhfs_destroy(void *);
int   			   lxhfs_mkdir(const char *, mode_t);
int   			   lxhfs_getattr(const char *, struct stat *);
int   			   lxhfs_readdir(const char *, void *, fuse_fill_dir_t, off_t,
						                struct fuse_file_info *);
int   			   lxhfs_mknod(const char *, mode_t, dev_t);
int   			   lxhfs_write(const char *, const char *, size_t, off_t,
					                  struct fuse_file_info *);
int   			   lxhfs_read(const char *, char *, size_t, off_t,
					                 struct fuse_file_info *);
int   			   lxhfs_access(const char *, int);
int   			   lxhfs_unlink(const char *);
int   			   lxhfs_rmdir(const char *);
int   			   lxhfs_rename(const char *, const char *);
int   			   lxhfs_utimens(const char *, const struct timespec tv[2]);
int   			   lxhfs_truncate(const char *, off_t);
			
int   			   lxhfs_open(const char *, struct fuse_file_info *);
int   			   lxhfs_opendir(const char *, struct fuse_file_info *);

#endif  /* _lxhfs_H_ */