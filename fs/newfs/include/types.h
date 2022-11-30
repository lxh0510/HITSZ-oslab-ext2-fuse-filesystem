#ifndef _TYPES_H_
#define _TYPES_H_

#define MAX_NAME_LEN    128  
typedef int          boolean;
typedef uint16_t     flag16;

typedef enum lxhfs_file_type {
    LXHFS_REG_FILE,
    LXHFS_DIR,
} LXHFS_FILE_TYPE;

/******************************************************************************
* SECTION: Macro
*******************************************************************************/
#define TRUE                    1
#define FALSE                   0
#define UINT32_BITS             32
#define UINT8_BITS              8

#define LXHFS_MAGIC_NUM           0x20020510 
#define LXHFS_SUPER_OFS           0
#define LXHFS_ROOT_INO            0



#define LXHFS_ERROR_NONE          0
#define LXHFS_ERROR_ACCESS        EACCES
#define LXHFS_ERROR_SEEK          ESPIPE     
#define LXHFS_ERROR_ISDIR         EISDIR
#define LXHFS_ERROR_NOSPACE       ENOSPC
#define LXHFS_ERROR_EXISTS        EEXIST
#define LXHFS_ERROR_NOTFOUND      ENOENT
#define LXHFS_ERROR_UNSUPPORTED   ENXIO
#define LXHFS_ERROR_IO            EIO     /* Error Input/Output */
#define LXHFS_ERROR_INVAL         EINVAL  /* Invalid Args */

#define LXHFS_MAX_FILE_NAME       128
#define LXHFS_INODE_PER_FILE      1
#define LXHFS_DATA_PER_FILE       16
#define LXHFS_DEFAULT_PERM        0777

#define LXHFS_IOC_MAGIC           'S'
#define LXHFS_IOC_SEEK            _IO(LXHFS_IOC_MAGIC, 0)

#define LXHFS_FLAG_BUF_DIRTY      0x1
#define LXHFS_FLAG_BUF_OCCUPY     0x2   

/******************************************************************************
* SECTION: Macro Function
*******************************************************************************/
#define LXHFS_IO_SZ()                     (lxhfs_super.sz_io)             /*inode的大小512B*/
#define LXHFS_BLK_SZ()                    (lxhfs_super.sz_blk)            /*EXT2文件系统一个块大小1024B*/
#define LXHFS_DISK_SZ()                   (lxhfs_super.sz_disk)           /*磁盘大小4MB*/
#define LXHFS_DRIVER()                    (lxhfs_super.driver_fd)

#define LXHFS_ROUND_DOWN(value, round)    (value % round == 0 ? value : (value / round) * round)
#define LXHFS_ROUND_UP(value, round)      (value % round == 0 ? value : (value / round + 1) * round)

#define LXHFS_BLKS_SZ(blks)               (blks * LXHFS_BLK_SZ())
#define LXHFS_ASSIGN_FNAME(plxhfs_dentry, _fname)   memcpy(plxhfs_dentry->fname, _fname, strlen(_fname))
#define LXHFS_INO_OFS(ino)                (lxhfs_super.inode_offset + ino * LXHFS_BLK_SZ())    /*求ino对应inode偏移位置*/
#define LXHFS_DATA_OFS(dno)               (lxhfs_super.data_offset + dno * LXHFS_BLK_SZ())     /*求dno对应data偏移位置*/

#define LXHFS_IS_DIR(pinode)              (pinode->dentry->ftype == LXHFS_DIR)
#define LXHFS_IS_REG(pinode)              (pinode->dentry->ftype == LXHFS_REG_FILE)
/******************************************************************************
* SECTION: FS Specific Structure - In memory structure 内存
*******************************************************************************/
struct lxhfs_super;
struct lxhfs_inode;
struct lxhfs_dentry;

struct custom_options {
	const char*        device;
	boolean            show_help;
};

struct lxhfs_super {
    /* TODO: Define yourself */
    int                driver_fd;

    int                sz_io;                   /*inode的大小*/
    int                sz_disk;                 /*磁盘大小*/
    int                sz_blk;                  /*EXT2文件系统一个块大小*/
    int                sz_usage;

    int                max_ino;                 /*inode的数目，即最多支持的文件数*/
    uint8_t*           map_inode;               /*inode位图*/
    int                map_inode_blks;          /*inode位图所占的数据块*/
    int                map_inode_offset;        /*inode位图的偏移,即起始地址*/

    int                max_data;               /*data索引的数目*/
    uint8_t*           map_data;               /*data位图*/
    int                map_data_blks;          /*数据位图所占的数据块*/
    int                map_data_offset;        /*数据位图的偏移,即起始地址*/

    int                inode_offset;            /*inode块区的偏移,即起始地址*/
    int                data_offset;             /*数据块的偏移,即起始地址*/

    boolean            is_mounted;

    struct lxhfs_dentry* root_dentry;             /*根目录*/
};

struct lxhfs_inode {
    /* TODO: Define yourself */
    uint32_t                ino;                           /* 在inode位图中的下标 */
    int                size;                          /* 文件已占用空间 */
    int                dir_cnt;                       /* 如果是目录类型文件，下面有几个目录项 */
    LXHFS_FILE_TYPE    ftype;                         /* 文件类型 */
    struct lxhfs_dentry* dentry;                      /* 指向该inode的dentry */
    struct lxhfs_dentry* dentrys;                     /* 所有目录项 */
    uint8_t*           data[LXHFS_DATA_PER_FILE];     /* 如果是FILE文件，数据块指针 */
    int                dno[LXHFS_DATA_PER_FILE];      /* inode指向文件的各个数据块在数据位图中的下标 */    
};

struct lxhfs_dentry {
    /* TODO: Define yourself */
    char    fname[LXHFS_MAX_FILE_NAME];
    LXHFS_FILE_TYPE    ftype;                         /* 文件类型 */
    struct lxhfs_dentry* parent;                        /* 父亲Inode的dentry */
    struct lxhfs_dentry* brother;                       /* 兄弟 */
    uint32_t     ino;                                      /* 指向的ino号 */
    struct lxhfs_inode*  inode;                         /* 指向inode */
    int     valid;                                    /* 该目录项是否有效 */  
};

static inline struct lxhfs_dentry* new_dentry(char * fname, LXHFS_FILE_TYPE ftype) {
    struct lxhfs_dentry * dentry = (struct lxhfs_dentry *)malloc(sizeof(struct lxhfs_dentry));
    memset(dentry, 0, sizeof(struct lxhfs_dentry));
    LXHFS_ASSIGN_FNAME(dentry, fname);
    dentry->ftype   = ftype;
    dentry->ino     = -1;
    dentry->inode   = NULL;
    dentry->parent  = NULL;
    dentry->brother = NULL;     
    return  dentry;                                       
}

/******************************************************************************
* SECTION: FS Specific Structure - Disk structure 磁盘
*******************************************************************************/
struct lxhfs_super_d {
    /* TODO: Define yourself */
    uint32_t           magic_num;
    int                sz_usage;

    uint32_t           max_ino;                 /*inode的数目，即最多支持的文件数*/
    int                map_inode_blks;          /*inode位图所占的数据块*/
    int                map_inode_offset;        /*inode位图的偏移*/

    int                map_data_blks;          /*数据位图所占的数据块*/
    int                map_data_offset;        /*数据位图的偏移*/

    int                inode_offset;            /*inode块的偏移*/
    int                data_offset;             /*数据块的偏移*/
};

struct lxhfs_inode_d {
    /* TODO: Define yourself */
    uint32_t                ino;                           /* 在inode位图中的下标 */
    int                size;                          /* 文件已占用空间 */
    int                dir_cnt;                       /* 如果是目录类型文件，下面有几个目录项 */
    LXHFS_FILE_TYPE    ftype;                         /* 文件类型 */
    int                dno[LXHFS_DATA_PER_FILE];      /* inode指向文件的各个数据块在数据位图中的下标 */    
};

struct lxhfs_dentry_d {
    /* TODO: Define yourself */
    char    fname[LXHFS_MAX_FILE_NAME];
    LXHFS_FILE_TYPE    ftype;                         /* 文件类型 */
    uint32_t     ino;                                      /* 指向的ino号 */
    int     valid;                                    /* 该目录项是否有效 */  
};
#endif /* _TYPES_H_ */