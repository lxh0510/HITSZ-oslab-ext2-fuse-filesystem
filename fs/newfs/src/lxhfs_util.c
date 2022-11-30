
#include "../include/lxhfs.h"

extern struct lxhfs_super      lxhfs_super; 
extern struct custom_options   lxhfs_options;

/**
 * @brief 获取文件名
 * 
 * @param path 
 * @return char* 
 */
char* lxhfs_get_fname(const char* path) {
    char ch = '/';
    char *q = strrchr(path, ch) + 1;
    return q;
}

/**
 * @brief 计算路径的层级
 * exm: /av/c/d/f
 * -> lvl = 4
 * @param path 
 * @return int 
 */
int lxhfs_calc_lvl(const char * path) {
    // char* path_cpy = (char *)malloc(strlen(path));
    // strcpy(path_cpy, path);
    char* str = path;
    int   lvl = 0;
    if (strcmp(path, "/") == 0) {
        return lvl;
    }
    while (*str != NULL) {
        if (*str == '/') {
            lvl++;
        }
        str++;
    }
    return lvl;
}

/**
 * @brief 驱动读
 * 
 * @param offset 
 * @param out_content 
 * @param size 
 * @return int 
 */
int lxhfs_driver_read(int offset, uint8_t *out_content, int size) {
    int      offset_aligned = LXHFS_ROUND_DOWN(offset, LXHFS_BLK_SZ());
    int      bias           = offset - offset_aligned;
    int      size_aligned   = LXHFS_ROUND_UP((size + bias), LXHFS_BLK_SZ());
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    // lseek(LXHFS_DRIVER(), offset_aligned, SEEK_SET);
    ddriver_seek(LXHFS_DRIVER(), offset_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        // read(LXHFS_DRIVER(), cur, LXHFS_BLK_SZ());
        ddriver_read(LXHFS_DRIVER(), cur, LXHFS_IO_SZ());                          // ddriver_read第三个参数size要等于设备IO单位的大小
        cur          += LXHFS_IO_SZ();
        size_aligned -= LXHFS_IO_SZ();   
    }
    memcpy(out_content, temp_content + bias, size);
    free(temp_content);
    return LXHFS_ERROR_NONE;
}
/**
 * @brief 驱动写
 * 
 * @param offset 
 * @param in_content 
 * @param size 
 * @return int 
 */
int lxhfs_driver_write(int offset, uint8_t *in_content, int size) {
    int      offset_aligned = LXHFS_ROUND_DOWN(offset, LXHFS_BLK_SZ());
    int      bias           = offset - offset_aligned;
    int      size_aligned   = LXHFS_ROUND_UP((size + bias), LXHFS_BLK_SZ());
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    lxhfs_driver_read(offset_aligned, temp_content, size_aligned);
    memcpy(temp_content + bias, in_content, size);
    
    // lseek(LXHFS_DRIVER(), offset_aligned, SEEK_SET);
    ddriver_seek(LXHFS_DRIVER(), offset_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        // write(LXHFS_DRIVER(), cur, LXHFS_IO_SZ());
        ddriver_write(LXHFS_DRIVER(), cur, LXHFS_IO_SZ());                           // ddriver_write第三个参数size要等于设备IO单位的大小
        cur          += LXHFS_IO_SZ();
        size_aligned -= LXHFS_IO_SZ();   
    }

    free(temp_content);
    return LXHFS_ERROR_NONE;
}

/**
 * @brief 为一个inode分配dentry，采用头插法
 * 
 * @param inode 
 * @param dentry 
 * @return int 
 */
int lxhfs_alloc_dentry(struct lxhfs_inode* inode, struct lxhfs_dentry* dentry) {
    if (inode->dentrys == NULL) {
        inode->dentrys = dentry;
    }
    else {
        dentry->brother = inode->dentrys;
        inode->dentrys = dentry;
    }
    inode->dir_cnt++;
    return inode->dir_cnt;
}

/**
 * @brief 分配一个inode，占用位图
 * 
 * @param dentry 该dentry指向分配的inode
 * @return lxhfs_inode
 */
struct lxhfs_inode* lxhfs_alloc_inode(struct lxhfs_dentry * dentry) {
    struct lxhfs_inode* inode;
    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int ino_cursor  = 0;
    int dno_cursor  = 0;
    int data_cnt    = 0;
    boolean is_find_free_entry = FALSE;
    boolean is_find_free_enough = FALSE;
    /*在inode位图上寻找未使用的inode节点*/
    for (byte_cursor = 0; byte_cursor < LXHFS_BLKS_SZ(lxhfs_super.map_inode_blks); 
         byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            if((lxhfs_super.map_inode[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                                                      /* 当前ino_cursor位置空闲 */
                lxhfs_super.map_inode[byte_cursor] |= (0x1 << bit_cursor);
                is_find_free_entry = TRUE;           
                break;
            }
            ino_cursor++;
        }
        if (is_find_free_entry) {
            break;
        }
    }

    /*为目录项分配inode节点*/
    if (!is_find_free_entry || ino_cursor == lxhfs_super.max_ino)
        return -LXHFS_ERROR_NOSPACE;

    inode = (struct lxhfs_inode*)malloc(sizeof(struct lxhfs_inode));
    inode->ino  = ino_cursor; 
    inode->size = 0;

    /*在data位图上寻找能容纳目录项的空闲data节点*/
    for (byte_cursor = 0; byte_cursor < LXHFS_BLKS_SZ(lxhfs_super.map_data_blks); 
         byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            if((lxhfs_super.map_data[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                                                      /* 当前dno_cursor位置空闲 */
                lxhfs_super.map_data[byte_cursor] |= (0x1 << bit_cursor);
                inode->dno[data_cnt++] = dno_cursor;  /*为目录项分配data节点*/
                if(data_cnt == LXHFS_DATA_PER_FILE){  /*由于一个目录项对应多个data块，因此空闲data节点足够时，停止寻找*/
                    is_find_free_enough = TRUE;
                    break;
                }           
            }
            dno_cursor++;
        }
        if (is_find_free_enough) {
            break;
        }
    }
    if (!is_find_free_enough || dno_cursor == lxhfs_super.max_data)
        return -LXHFS_ERROR_NOSPACE;
    /*为目录项分配inode节点并建立他们之间的连接*/
                                                      /* dentry指向inode */
    dentry->inode = inode;
    dentry->ino   = inode->ino;

                                                      /* inode指回dentry */
    inode->dentry = dentry;
    
    inode->dir_cnt = 0;
    inode->dentrys = NULL;

    /*对于FILE类型要给其分配空间*/
    if (LXHFS_IS_REG(inode)) {
        int cnt;
        for(cnt=0;cnt<LXHFS_DATA_PER_FILE;cnt++){
            inode->data[cnt] = (uint8_t *)malloc(LXHFS_BLK_SZ());
        }
    }

    return inode;
}

/**
 * @brief 将内存inode及其下方结构全部刷回磁盘
 * 
 * @param inode 
 * @return int 
 */
int lxhfs_sync_inode(struct lxhfs_inode * inode) {
    struct lxhfs_inode_d  inode_d;
    struct lxhfs_dentry*  dentry_cursor;
    struct lxhfs_dentry_d dentry_d;
    int ino             = inode->ino;
    inode_d.ino         = ino;
    inode_d.size        = inode->size;
    inode_d.ftype       = inode->dentry->ftype;
    inode_d.dir_cnt     = inode->dir_cnt;
    int dno_cnt;
    for(dno_cnt=0;dno_cnt<LXHFS_DATA_PER_FILE;dno_cnt++){
        inode_d.dno[dno_cnt] = inode->dno[dno_cnt];
    }
    int offset;
    
    if (lxhfs_driver_write(LXHFS_INO_OFS(ino), (uint8_t *)&inode_d, 
                     sizeof(struct lxhfs_inode_d)) != LXHFS_ERROR_NONE) {
        LXHFS_DBG("[%s] io error\n", __func__);
        return -LXHFS_ERROR_IO;
    }

                                                      /* Cycle 1: 写 INODE */
                                                      /* Cycle 2: 写 数据 */
    if (LXHFS_IS_DIR(inode)) {       
        dno_cnt = 0;                   
        dentry_cursor = inode->dentrys;
        while (dentry_cursor != NULL && dno_cnt<LXHFS_DATA_PER_FILE){
            offset = LXHFS_DATA_OFS(inode->dno[dno_cnt]);
            /*逐层进行目录的写入*/
            while (dentry_cursor != NULL)
            {
                memcpy(dentry_d.fname, dentry_cursor->fname, LXHFS_MAX_FILE_NAME);
                dentry_d.ftype = dentry_cursor->ftype;
                dentry_d.ino = dentry_cursor->ino;
                if (lxhfs_driver_write(offset, (uint8_t *)&dentry_d, 
                                    sizeof(struct lxhfs_dentry_d)) != LXHFS_ERROR_NONE) {
                    LXHFS_DBG("[%s] io error\n", __func__);
                    return -LXHFS_ERROR_IO;                     
                }
            
                if (dentry_cursor->inode != NULL) {
                    lxhfs_sync_inode(dentry_cursor->inode);
                }

                dentry_cursor = dentry_cursor->brother;
                offset += sizeof(struct lxhfs_dentry_d);
                if((offset+sizeof(struct lxhfs_dentry_d))>LXHFS_DATA_OFS(inode->dno[dno_cnt]+1)){             /*如果一个数据块不够写，写到下一个数据块*/
                    break;
                }
            }
            dno_cnt++;
        }
    }
    else if (LXHFS_IS_REG(inode)) {
        /*inode对应文件格式的写入*/
        for(dno_cnt=0;dno_cnt<LXHFS_DATA_PER_FILE;dno_cnt++){
            if (lxhfs_driver_write(LXHFS_DATA_OFS(inode->dno[dno_cnt]), inode->data[dno_cnt], 
                                LXHFS_BLK_SZ()) != LXHFS_ERROR_NONE) {
                LXHFS_DBG("[%s] io error\n", __func__);
                return -LXHFS_ERROR_IO;
            }
        }
    }
    return LXHFS_ERROR_NONE;
}

/**
 * @brief 
 * 
 * @param dentry dentry指向ino，读取该inode
 * @param ino inode唯一编号
 * @return struct lxhfs_inode* 
 */
struct lxhfs_inode* lxhfs_read_inode(struct lxhfs_dentry * dentry, int ino) {
    struct lxhfs_inode* inode = (struct lxhfs_inode*)malloc(sizeof(struct lxhfs_inode));
    struct lxhfs_inode_d inode_d;
    struct lxhfs_dentry* sub_dentry;
    struct lxhfs_dentry_d dentry_d;
    int    dir_cnt = 0, i , dno_cnt ,off_cnt;

    /*通过磁盘驱动来将磁盘中ino号的inode读入内存*/
    if (lxhfs_driver_read(LXHFS_INO_OFS(ino), (uint8_t *)&inode_d, 
                        sizeof(struct lxhfs_inode_d)) != LXHFS_ERROR_NONE) {
        LXHFS_DBG("[%s] io error\n", __func__);
        return NULL;                    
    }
    inode->dir_cnt = 0;
    inode->ino = inode_d.ino;
    inode->size = inode_d.size;
    inode->dentry = dentry;
    inode->dentrys = NULL;
    for(dno_cnt=0;dno_cnt<LXHFS_DATA_PER_FILE;dno_cnt++){
        inode->dno[dno_cnt] = inode_d.dno[dno_cnt];
    }

    /*此处实现方式类似sync_icode，分两种文件类型分别讨论*/
    /*若是目录类型*/
    if (LXHFS_IS_DIR(inode)) {
        dir_cnt = inode_d.dir_cnt;
        dno_cnt = 0;
        i = 0;
        off_cnt = 0;
        while(i<dir_cnt&&dno_cnt<LXHFS_DATA_PER_FILE)
        {
            if (lxhfs_driver_read(LXHFS_DATA_OFS(inode->dno[dno_cnt]) + off_cnt * sizeof(struct lxhfs_dentry_d), 
                                (uint8_t *)&dentry_d, 
                                sizeof(struct lxhfs_dentry_d)) != LXHFS_ERROR_NONE) {
                LXHFS_DBG("[%s] io error\n", __func__);
                return NULL;                    
            }
            sub_dentry = new_dentry(dentry_d.fname, dentry_d.ftype);
            sub_dentry->parent = inode->dentry;
            sub_dentry->ino    = dentry_d.ino; 
            lxhfs_alloc_dentry(inode, sub_dentry);
            i++;
            off_cnt++;
            /*如果一个数据块不够写，写到下一个数据块*/
            if(LXHFS_DATA_OFS(inode->dno[dno_cnt]) + (off_cnt+1) * sizeof(struct lxhfs_dentry_d)>LXHFS_DATA_OFS(inode->dno[dno_cnt]+1)){  
                dno_cnt++;
                off_cnt = 0;
            }
        }
    }
    /*若是文件类型直接读取数据即可*/
    else if (LXHFS_IS_REG(inode)) {
        /*对于文件的每一个数据块分别进行读取*/
        for(dno_cnt=0;dno_cnt<LXHFS_DATA_PER_FILE;dno_cnt++){
            inode->data[dno_cnt] = (uint8_t *)malloc(LXHFS_BLK_SZ());
            if (lxhfs_driver_read(LXHFS_DATA_OFS(inode->dno[dno_cnt]), (uint8_t *)inode->data[dno_cnt], 
                                LXHFS_BLK_SZ()) != LXHFS_ERROR_NONE) {
                LXHFS_DBG("[%s] io error\n", __func__);
                return NULL;                    
            }
        }
    }
    return inode;
}

/**
 * @brief 获得inode节点对应的dentry
 * 
 * @param inode 
 * @param dir [0...]
 * @return struct lxhfs_dentry* 
 */
struct lxhfs_dentry* lxhfs_get_dentry(struct lxhfs_inode * inode, int dir) {
    struct lxhfs_dentry* dentry_cursor = inode->dentrys;
    int    cnt = 0;
    while (dentry_cursor)
    {
        if (dir == cnt) {
            return dentry_cursor;
        }
        cnt++;
        dentry_cursor = dentry_cursor->brother;
    }
    return NULL;
}

/**
 * @brief 
 * 路径解析函数，返回匹配的dentry
 * path: /qwe/ad  total_lvl = 2,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry 
 *      3) find qwe's inode     lvl = 2
 *      4) find ad's dentry
 *
 * path: /qwe     total_lvl = 1,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry
 * 
 * @param path 
 * @return struct lxhfs_inode* 
 */
struct lxhfs_dentry* lxhfs_lookup(const char * path, boolean* is_find, boolean* is_root) {
    struct lxhfs_dentry* dentry_cursor = lxhfs_super.root_dentry;
    struct lxhfs_dentry* dentry_ret = NULL;
    struct lxhfs_inode*  inode; 
    int   total_lvl = lxhfs_calc_lvl(path);
    int   lvl = 0;
    boolean is_hit;
    char* fname = NULL;
    char* path_cpy = (char*)malloc(sizeof(path));
    *is_root = FALSE;
    strcpy(path_cpy, path);                         /*分析路径函数*/

    if (total_lvl == 0) {                           /* 根目录 */
        *is_find = TRUE;
        *is_root = TRUE;
        dentry_ret = lxhfs_super.root_dentry;
    }
    fname = strtok(path_cpy, "/");       
    while (fname)
    {   
        lvl++;
        if (dentry_cursor->inode == NULL) {           /* Cache机制 */
            lxhfs_read_inode(dentry_cursor, dentry_cursor->ino);
        }

        inode = dentry_cursor->inode;
        /*若遍历到的inode节点是FILE类型，则结束遍历*/
        if (LXHFS_IS_REG(inode) && lvl < total_lvl) {
            LXHFS_DBG("[%s] not a dir\n", __func__);
            dentry_ret = inode->dentry;
            break;
        }
        /*若遍历到的inode节点是目录类型*/
        if (LXHFS_IS_DIR(inode)) {
            dentry_cursor = inode->dentrys;
            is_hit        = FALSE;

            while (dentry_cursor)
            {
                if (memcmp(dentry_cursor->fname, fname, strlen(fname)) == 0) {
                    is_hit = TRUE;
                    break;
                }
                dentry_cursor = dentry_cursor->brother;            /*遍历目录的子文件*/
            }
            /*未找到匹配路径*/
            if (!is_hit) {
                *is_find = FALSE;
                LXHFS_DBG("[%s] not found %s\n", __func__, fname);
                dentry_ret = inode->dentry;
                break;
            }
            /*找到完整匹配路径*/
            if (is_hit && lvl == total_lvl) {
                *is_find = TRUE;
                dentry_ret = dentry_cursor;
                break;
            }
        }
        fname = strtok(NULL, "/"); 
    }
    /*若函数运行时inode还未读进来，则需要重新读*/
    if (dentry_ret->inode == NULL) {
        dentry_ret->inode = lxhfs_read_inode(dentry_ret, dentry_ret->ino);
    }
    
    return dentry_ret;
}

/**
 * @brief 挂载lxhfs, Layout 如下
 * 
 * Layout
 * | Super | Inode Map | Data |
 * 
 * BLK_SZ = 2*Inode_SZ
 * 
 * 每个Inode占用一个Blk
 * @param options 
 * @return int 
 */
int lxhfs_mount(struct custom_options options){
    /*定义磁盘各部分结构*/
    int                 ret = LXHFS_ERROR_NONE;
    int                 driver_fd;
    struct lxhfs_super_d  lxhfs_super_d; 
    struct lxhfs_dentry*  root_dentry;
    struct lxhfs_inode*   root_inode;

    int                 inode_num;
    int                 data_num;
    int                 map_inode_blks;
    int                 map_data_blks;
    
    int                 super_blks;
    boolean             is_init = FALSE;

    lxhfs_super.is_mounted = FALSE;

    // driver_fd = open(options.device, O_RDWR);
    driver_fd = ddriver_open(options.device);   /*打开驱动*/

    if (driver_fd < 0) {
        return driver_fd;
    }

    /* 向内存超级块中标记驱动并写入磁盘大小和单次IO大小*/
    lxhfs_super.driver_fd = driver_fd;
    ddriver_ioctl(LXHFS_DRIVER(), IOC_REQ_DEVICE_SIZE,  &lxhfs_super.sz_disk);
    ddriver_ioctl(LXHFS_DRIVER(), IOC_REQ_DEVICE_IO_SZ, &lxhfs_super.sz_io);

    lxhfs_super.sz_blk = 2*lxhfs_super.sz_io;
    /*创建根目录项并读取磁盘超级块到内存*/
    root_dentry = new_dentry("/", LXHFS_DIR);

    if (lxhfs_driver_read(LXHFS_SUPER_OFS, (uint8_t *)(&lxhfs_super_d), 
                        sizeof(struct lxhfs_super_d)) != LXHFS_ERROR_NONE) {
        return -LXHFS_ERROR_IO;
    }   

    /*根据超级块幻数判断是否为第一次启动磁盘，如果是第一次启动磁盘，则需要建立磁盘超级块的布局*/
                                                      /* 读取super */
    if (lxhfs_super_d.magic_num != LXHFS_MAGIC_NUM) {     /* 幻数无 */

        /* 为了简单起见，我们可以自行 规定位图 的大小 */
        super_blks = 1;
        inode_num  =  128;
        data_num = 512;
        map_inode_blks = 1;
        map_data_blks = 1;
        
                                                      /* 布局layout */
        lxhfs_super.max_ino = inode_num ; 
        lxhfs_super.max_data = data_num ; 
        
        lxhfs_super_d.map_inode_offset = LXHFS_SUPER_OFS + LXHFS_BLKS_SZ(super_blks);
        lxhfs_super_d.map_data_offset = lxhfs_super_d.map_inode_offset + LXHFS_BLKS_SZ(map_inode_blks);
        lxhfs_super_d.inode_offset = lxhfs_super_d.map_data_offset + LXHFS_BLKS_SZ(map_data_blks);
        lxhfs_super_d.data_offset = lxhfs_super_d.inode_offset + LXHFS_BLKS_SZ(inode_num);

        lxhfs_super_d.map_inode_blks  = map_inode_blks;
        lxhfs_super_d.map_data_blks  = map_data_blks;
        
        lxhfs_super_d.magic_num    = LXHFS_MAGIC_NUM;
        lxhfs_super_d.sz_usage    = 0;
        LXHFS_DBG("inode map blocks: %d\n", map_inode_blks);
        is_init = TRUE;
    }

    /*初始化内存中的超级块，和根目录项*/
    lxhfs_super.sz_usage   = lxhfs_super_d.sz_usage;      /* 建立 in-memory 结构 */
    
    lxhfs_super.map_inode = (uint8_t *)malloc(LXHFS_BLKS_SZ(lxhfs_super_d.map_inode_blks));
    lxhfs_super.map_data = (uint8_t *)malloc(LXHFS_BLKS_SZ(lxhfs_super_d.map_data_blks));
    lxhfs_super.map_inode_blks = lxhfs_super_d.map_inode_blks;
    lxhfs_super.map_data_blks = lxhfs_super_d.map_data_blks;
    lxhfs_super.map_inode_offset = lxhfs_super_d.map_inode_offset;
    lxhfs_super.map_data_offset = lxhfs_super_d.map_data_offset;
    lxhfs_super.inode_offset = lxhfs_super_d.inode_offset;
    lxhfs_super.data_offset = lxhfs_super_d.data_offset;

    if (lxhfs_driver_read(lxhfs_super_d.map_inode_offset, (uint8_t *)(lxhfs_super.map_inode), 
                        LXHFS_BLKS_SZ(lxhfs_super_d.map_inode_blks)) != LXHFS_ERROR_NONE) {
        return -LXHFS_ERROR_IO;
    }
    if (lxhfs_driver_read(lxhfs_super_d.map_data_offset, (uint8_t *)(lxhfs_super.map_data), 
                        LXHFS_BLKS_SZ(lxhfs_super_d.map_data_blks)) != LXHFS_ERROR_NONE) {
        return -LXHFS_ERROR_IO;
    }

    if (is_init) {                                    /* 分配根节点 */
        root_inode = lxhfs_alloc_inode(root_dentry);
        lxhfs_sync_inode(root_inode);                 /*将根节点写回磁盘*/
    }
    
    root_inode            = lxhfs_read_inode(root_dentry, LXHFS_ROOT_INO);
    root_dentry->inode    = root_inode;
    lxhfs_super.root_dentry = root_dentry;
    lxhfs_super.is_mounted  = TRUE;

    return ret;
}

/**
 * @brief 
 * 
 * @return int 
 */
int lxhfs_umount() {
    struct lxhfs_super_d  lxhfs_super_d; 

    if (!lxhfs_super.is_mounted) {
        return LXHFS_ERROR_NONE;
    }

    lxhfs_sync_inode(lxhfs_super.root_dentry->inode);     /* 从根节点向下刷写节点 */

    /*将内存超级块转换为磁盘超级块并写入磁盘*/                                                
    lxhfs_super_d.magic_num           = LXHFS_MAGIC_NUM;
    lxhfs_super_d.map_inode_blks      = lxhfs_super.map_inode_blks;
    lxhfs_super_d.map_data_blks       = lxhfs_super.map_data_blks;
    lxhfs_super_d.map_inode_offset    = lxhfs_super.map_inode_offset;
    lxhfs_super_d.map_data_offset     = lxhfs_super.map_data_offset;
    lxhfs_super_d.inode_offset        = lxhfs_super.inode_offset;
    lxhfs_super_d.data_offset         = lxhfs_super.data_offset;
    lxhfs_super_d.sz_usage            = lxhfs_super.sz_usage;

    if (lxhfs_driver_write(LXHFS_SUPER_OFS, (uint8_t *)&lxhfs_super_d, 
                     sizeof(struct lxhfs_super_d)) != LXHFS_ERROR_NONE) {
        return -LXHFS_ERROR_IO;
    }

    /*将inode位图和data位图写入磁盘*/
    if (lxhfs_driver_write(lxhfs_super_d.map_inode_offset, (uint8_t *)(lxhfs_super.map_inode), 
                         LXHFS_BLKS_SZ(lxhfs_super_d.map_inode_blks)) != LXHFS_ERROR_NONE) {
        return -LXHFS_ERROR_IO;
    }

    if (lxhfs_driver_write(lxhfs_super_d.map_data_offset, (uint8_t *)(lxhfs_super.map_data), 
                         LXHFS_BLKS_SZ(lxhfs_super_d.map_data_blks)) != LXHFS_ERROR_NONE) {
        return -LXHFS_ERROR_IO;
    }

    free(lxhfs_super.map_inode);
    free(lxhfs_super.map_data);

    /*关闭驱动*/
    ddriver_close(LXHFS_DRIVER());

    return LXHFS_ERROR_NONE;
}
