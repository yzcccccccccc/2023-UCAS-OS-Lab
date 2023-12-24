#ifndef _FS_H
#define _FS_H
/********************************************************
            Disk Layout (SD card :D)

--------------------- Kernel ---------------------
|   Name    |   Sector_num  |   Sector_start    |
| bootblock |       1       |       0           |
| kernel    |       x       |       1           |
| app_info  |       x       |   behind kernel   |
| app       |       x       |   behind app_info |
| swp area  |       x       |   behind app      |

--------------------------- FS ---------------------------
|   Name            |   Block_num   |   Sector_start    |
|   Super Block     |       1       |       2^20        |
|   inode Bitmap    |       1       |       2^20+8      |
|   data  Bitmap    |       32      |       2^20+16     |
|   inode           |       1024    |       2^20+272    |
|   data            |       X       |       2^20+8464   |
    
*********************************************************/
#include <type.h>

#define NUM_FDESCS      32                  // total num of file descriptors

// super block params
#define FS_SUPER_MAGIC      0x114514
#define FS_INODEMAP_OFFSET  8               // offset from superblock (sectors)
#define FS_DATAMAP_OFFSET   16
#define FS_INODE_OFFSET     272
#define FS_DATA_OFFSET      8466
#define FS_SEC_START        1048576         // 512 MB
#define FS_INODE_SIZE       128
#define FS_DENTRY_SIZE      32
#define FS_INODE_BLKNUM     1024
#define FS_INODE_SECNUM     8192            // 1024 * 8
#define FS_DATA_SECNUM      8388608         // 1048576 * 8

#define FS_SUPER_SECNUM     1
#define FS_INODEMAP_BLKNUM  1
#define FS_INODEMAP_SECNUM  8
#define FS_DATAMAP_BLKNUM   32
#define FS_DATAMAP_SECNUM   256         // 32 * 8

#define FS_SECTOR_SIZE      512
#define FS_BLOCK_SIZE       4096
#define FS_BLOCK_SECNUM     8           // 512 * 8 = 4096

#define FS_INO_NUM          32768      // 4096 * 8

#define DIRECT_NUM      10
#define INDIRECT1_NUM   3
#define INDIRECT2_NUM   2
#define INDIRECT3_NUM   1

typedef enum inode_type{
    I_DIR,
    I_FILE,
}inode_type_t;

typedef enum dentry_type{
    D_NULL,
    D_DIR,
    D_FILE,
}dentry_type_t;

typedef enum file_access{
    O_RO,
    O_WO,
    O_RW,
}file_access_t;

//---------------------- Super Block ----------------------
typedef struct super_block{
    uint32_t magic;
    uint32_t fs_start_sector;

    uint32_t bbitmap_start;
    uint32_t bbitmap_secNum;

    uint32_t ibitmap_start;
    uint32_t ibitmap_secNum;

    uint32_t inode_start;
    uint32_t inode_secNum;

    uint32_t data_start;
    uint32_t data_secNum;

    uint32_t inode_size;
    uint32_t dentry_size;

    char padding[464];                  // pad up to 512 Bytes
}super_block_t;
extern super_block_t SuperBlock;

//---------------------- dentry ----------------------
typedef struct dentry{          // size: 32 Bytes
    int ino;
    dentry_type_t dtype;
    char name[24];
}dentry_t;

//---------------------- inode ----------------------
typedef struct inode{           // size: 128 Bytes
    int ino;
    uint32_t pino;          // parent

    inode_type_t type;
    file_access_t access;

    uint32_t link_cnt;
    uint32_t file_size;
    uint32_t create_time;
    uint32_t modify_time;

    uint32_t direct[DIRECT_NUM];
    uint32_t indirect1[INDIRECT1_NUM];
    uint32_t indirect2[INDIRECT2_NUM];
    uint32_t indirect3[INDIRECT3_NUM];

    char name[32];
}inode_t;

//---------------------- File Desc ----------------------
typedef struct fsdesc{
    uint32_t ino;
    file_access_t access;
    uint32_t rp, wp;
    uint32_t owner_pid;
}fsdesc_t;
extern fsdesc_t fdtable[NUM_FDESCS];

#define FS_PATH_DEPTH   20
#define FS_NAME_LEN     20

extern void fs_read_block(int sec_offset, void *dest);
extern void fs_write_block(int sec_offset, void *src);
extern void fs_read_sector(int sec_offset, void *dest);
extern void fs_write_sector(int sec_offset, void *src);
extern void fs_clearBlk(int sec_offset);
extern void fs_clearSec(int sec_offset);
extern int fs_addBlk_ino(int cur_ino);
extern int fs_addBlk_ptr(inode_t *inode_ptr);
extern int fs_mk_dentry(inode_t *inode_ptr, dentry_type_t dtype, char *name, int target_ino);
extern int fs_rm_dentry();
extern int fs_get_file_blk(int blk_index, inode_t *inode_ptr);

extern void do_mkfs(int force);
extern void do_statfs();
extern int do_cd(char *path);
extern int do_ls(int mode, char *path);
extern int do_mkdir(char *path);
extern void do_pwd();
#endif