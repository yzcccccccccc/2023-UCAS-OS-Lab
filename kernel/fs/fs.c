/******************************************************
    A simple file system.
    Authored by yzcc, 2023.12.20
*******************************************************/
#include <os/fs.h>
#include <os/kernel.h>
#include <os/string.h>
#include <os/smp.h>
#include <os/time.h>
#include <os/sched.h>
#include <assert.h>
#include <stdrtv.h>
#include <printk.h>
#include <pgtable.h>
#include <screen.h>

fsdesc_t fdtable[NUM_FDESCS];

super_block_t SuperBlock;
int datamap_id = -1;
int inodemap_valid = 0;
char data_bitmap_buf[FS_BLOCK_SIZE];
char inode_bitmap_buf[FS_BLOCK_SIZE];
const char clean_blk[FS_BLOCK_SIZE]={0};

void fs_read_superblock(){
    fs_read_sector(0, (void *)&SuperBlock);
}
void fs_write_superblock(){
    fs_write_sector(0, (void *)&SuperBlock);
}
void fs_read_inodemap(){
    if (!inodemap_valid){
        fs_read_block(FS_INODEMAP_OFFSET, (void *)inode_bitmap_buf);
        inodemap_valid = 1;
    }
}
void fs_write_inodemap(){
    fs_write_block(FS_INODEMAP_OFFSET, (void *)inode_bitmap_buf);
    inodemap_valid = 0;
}
void fs_read_datamap(int id){      // id = 0, 1, 2, 3, ... 31
    if (datamap_id != id){
        datamap_id = id;
        fs_read_block(FS_DATAMAP_OFFSET+id*FS_BLOCK_SECNUM, (void *)data_bitmap_buf);
    }
}
void fs_write_datamap(int id){
    fs_write_block(FS_DATAMAP_OFFSET+id*FS_BLOCK_SECNUM, (void *)data_bitmap_buf);
    if (datamap_id == id)
        datamap_id = -1;
}

// sec_offset: offset from FS_SEC_START
void fs_read_block(int sec_offset, void *dest){
    bios_sd_read(kva2pa((uintptr_t)dest), FS_BLOCK_SECNUM, FS_SEC_START + sec_offset);
}
void fs_write_block(int sec_offset, void *src){
    bios_sd_write(kva2pa((uintptr_t)src), FS_BLOCK_SECNUM, FS_SEC_START + sec_offset);
}
void fs_read_sector(int sec_offset, void *dest){
    bios_sd_read(kva2pa((uintptr_t)dest), 1, FS_SEC_START + sec_offset);
}
void fs_write_sector(int sec_offset, void *src){
    bios_sd_write(kva2pa((uintptr_t)src), 1, FS_SEC_START + sec_offset);
}

void fs_read_inode(inode_t *dest, uint32_t ino){
    char tmp_sec[FS_SECTOR_SIZE];
    // 1 sector = 512 bytes = 4 inodes, group inodes by 4
    int group = ino / 4, offset = (ino % 4) * FS_INODE_SIZE;
    int sec_offset = FS_INODE_OFFSET + group;
    fs_read_sector(sec_offset, (void *)tmp_sec);
    memcpy((uint8_t *)dest, (uint8_t *)(tmp_sec + offset), FS_INODE_SIZE);
}
void fs_write_inode(inode_t *src, uint32_t ino){
    char tmp_sec[FS_SECTOR_SIZE];
    int group = ino / 4, offset = (ino % 4) * FS_INODE_SIZE;
    int sec_offset = FS_INODE_OFFSET + group;
    fs_read_sector(sec_offset, (void *)tmp_sec);
    memcpy((uint8_t *)(tmp_sec + offset), (uint8_t *)src, FS_INODE_SIZE);
    fs_write_sector(sec_offset, (void *)tmp_sec);
}

void fs_clearBlk(int sec_offset){
    fs_write_block(sec_offset, (void *)clean_blk);
}
void fs_clearSec(int sec_offset){
    fs_write_sector(sec_offset, (void *)clean_blk);
}

uint32_t byte_bits[8] = {0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80};
// [p6] allocate inode, return ino if succeed, return -1 if fail
int alloc_inode(){
    fs_read_inodemap();
    for (int i = 0; i < FS_BLOCK_SIZE; i++){
        if (inode_bitmap_buf[i] != 0xff){
            int bit_cur;
            for (bit_cur = 0; bit_cur < 8; bit_cur++){
                if ((inode_bitmap_buf[i] & byte_bits[bit_cur]) == 0){
                    inode_bitmap_buf[i] |= byte_bits[bit_cur];
                    fs_write_inodemap();

                    inode_t tmp_inode;
                    tmp_inode.ino = i * 8 + bit_cur;
                    memset(tmp_inode.direct, 0, sizeof(int) * DIRECT_NUM);
                    memset(tmp_inode.indirect1, 0, sizeof(int) * INDIRECT1_NUM);
                    memset(tmp_inode.indirect2, 0, sizeof(int) * INDIRECT2_NUM);
                    memset(tmp_inode.indirect3, 0, sizeof(int) * INDIRECT3_NUM);
                    fs_write_inode(&tmp_inode, tmp_inode.ino);

                    return tmp_inode.ino;
                }
            }
        }
    }
    return -1;
}

// [p6] allcate data block, return data block start sector if succeed, return -1 if fail
int alloc_datablk(){
    for (int id = 0; id < FS_DATAMAP_BLKNUM; id++){
        fs_read_datamap(id);
        for (int i = 0; i < FS_BLOCK_SIZE; i++){
            if (data_bitmap_buf[i] != 0xff){
                int bit_cur;
                for (bit_cur = 0; bit_cur < 8; bit_cur++){
                    if ((data_bitmap_buf[i] & byte_bits[bit_cur]) == 0){
                        data_bitmap_buf[i] |= byte_bits[bit_cur];
                        fs_write_datamap(id);
                        return (id * 4096 * 8 + i * 8 + bit_cur) * FS_BLOCK_SECNUM + FS_DATA_OFFSET;
                    }
                }
            }
        }
    }
    return -1;
}

void release_inode(int ino){
    int index = ino / 8;
    int bit_cur = ino % 8;
    fs_read_inodemap();
    inode_bitmap_buf[index] &= ~(byte_bits[bit_cur]);
    fs_write_inodemap();
}

void release_datablk(int sec_offset){
    int bit_cur = (sec_offset - FS_DATA_OFFSET) / FS_BLOCK_SECNUM % 8;
    int id = (sec_offset - FS_DATA_OFFSET) / FS_BLOCK_SECNUM / (4096 * 8);
    int index = (sec_offset - FS_DATA_OFFSET) / FS_BLOCK_SECNUM % (4096 * 8) / 8;

    fs_read_datamap(id);
    data_bitmap_buf[index] &= ~(byte_bits[bit_cur]);
    fs_write_datamap(id);
}

// [p6] adding a data block for cur_ino
int fs_addBlk_ino(int cur_ino){
    inode_t tmp_inode;
    fs_read_inode(&tmp_inode, cur_ino);
    return fs_addBlk_ptr(&tmp_inode);
}

// [p6] walk! add a block (future: delete blocks, look up blocks)
// target_lev: 0~3, index_list: array of sector id, lev0_num: num of items in level0
// cur_lev: current level, par_sec: parent sector id
#define FS_WALK_ADD 0
#define FS_WALK_DEL 1
#define FS_WALK_QUERY 2
int walkthrough_index(int target_lev, int *index_list, int lev0_num, int cur_lev, int par_sec, int mode,
                        dentry_t *target_dentry){
    int ITE_BOUND = cur_lev == 0 ? lev0_num : FS_BLOCK_SIZE / sizeof(int), rtv = -1;
    if (cur_lev == target_lev){
        if (mode == FS_WALK_ADD){
            for (int i = 0, data_sec; i < ITE_BOUND; i++){
                if (index_list[i] == 0){
                    data_sec = alloc_datablk();
                    index_list[i] = data_sec;
                    fs_clearBlk(data_sec);
                    if (cur_lev != 0)
                        fs_write_block(par_sec, index_list);
                    rtv = data_sec;
                    break;
                }
            }
        }
        if (mode == FS_WALK_DEL){
            for (int i = 0; i < ITE_BOUND; i++){
                if (index_list[i] != 0){
                    release_datablk(index_list[i]);
                    index_list[i] = 0;
                }
            }
        }
        if (mode == FS_WALK_QUERY){
            char wk_blk[FS_BLOCK_SIZE];
            dentry_t *dentry_ptr;
            for (int i = 0; i < ITE_BOUND; i++){
                if (index_list[i] != 0){
                    fs_read_block(index_list[i], wk_blk);
                    dentry_ptr = (dentry_t *)wk_blk;
                    for (int j = 0; j < FS_BLOCK_SIZE / FS_DENTRY_SIZE; j++){
                        if (dentry_ptr[j].dtype == target_dentry->dtype && !strcmp(dentry_ptr[j].name, target_dentry->name)){
                            return rtv = dentry_ptr[j].ino;
                        }
                    }
                }
            }
        }
    }
    else {
        char wk_blk[FS_BLOCK_SIZE];
        for (int i = 0, data_sec; i < ITE_BOUND; i++){
            if (index_list[i] == 0 && mode == FS_WALK_ADD){
                data_sec = alloc_datablk();
                index_list[i] = data_sec;
                fs_clearBlk(data_sec);
                if (cur_lev != 0)
                    fs_write_block(par_sec, index_list);
            }
            if (index_list[i] != 0){
                fs_read_block(index_list[i], wk_blk);
                rtv = walkthrough_index(target_lev, (int *)wk_blk, lev0_num, cur_lev + 1, index_list[i], mode, target_dentry);
                if (mode == FS_WALK_DEL){
                    release_datablk(index_list[i]);
                    index_list[i] = 0;
                }
            }
        }
    }
    return rtv;
}

// [p6] adding a data block
int fs_addBlk_ptr(inode_t *inode_ptr){
    int lev0_num, rtv = -1;
    // Direct
    lev0_num = DIRECT_NUM;
    rtv = walkthrough_index(0, (int *)inode_ptr->direct, lev0_num, 0, 0, FS_WALK_ADD, NULL);
    if (rtv != -1){
        fs_write_inode(inode_ptr, inode_ptr->ino);
        return rtv;
    }

    // Indirect1
    lev0_num = INDIRECT1_NUM;
    rtv = walkthrough_index(1, (int *)inode_ptr->indirect1, lev0_num, 0, 0, FS_WALK_ADD, NULL);
    if (rtv != -1){
        fs_write_inode(inode_ptr, inode_ptr->ino);
        return rtv;
    }

    // Indirect2
    lev0_num = INDIRECT2_NUM;
    rtv = walkthrough_index(2, (int *)inode_ptr->indirect2, lev0_num, 0, 0, FS_WALK_ADD, NULL);
   if (rtv != -1){
        fs_write_inode(inode_ptr, inode_ptr->ino);
        return rtv;
    }

    // Indirect3
    lev0_num = INDIRECT3_NUM;
    rtv = walkthrough_index(3, (int *)inode_ptr->indirect3, lev0_num, 0, 0, FS_WALK_ADD, NULL);
    if (rtv != -1){
        fs_write_inode(inode_ptr, inode_ptr->ino);
        return rtv;
    }
    return rtv;
}

// [p6] adding a file
// allocate 4KB data-blk for dir during fs_addFile(), while File doesn't have that. 
int fs_addFile(inode_t *cur_inode, inode_type_t type, file_access_t access, char *name){
    int ino = alloc_inode();
    if (ino == -1)  return -1;      // Fail, ino insufficient. :(

    inode_t tmp_inode;
    fs_read_inode(&tmp_inode, ino);

    // basic info
    tmp_inode.ino = ino;
    tmp_inode.create_time = tmp_inode.modify_time = get_timer();
    tmp_inode.pino = cur_inode->ino;
    tmp_inode.type = type;
    tmp_inode.access = access;
    tmp_inode.file_size = 0;
    tmp_inode.link_cnt = 0;

    // hard-link
    if (type == I_DIR){
        fs_addBlk_ptr(&tmp_inode);
        tmp_inode.file_size = FS_BLOCK_SIZE;

        fs_mk_dentry(&tmp_inode, D_DIR, ".", ino);
        fs_mk_dentry(&tmp_inode, D_DIR, "..", cur_inode->ino);
    }

    // name
    strcpy(tmp_inode.name, name);

    fs_write_inode(&tmp_inode, ino);
    return ino;
}

// [p6] insert a dir entry
int fs_mk_dentry(inode_t *inode_ptr, dentry_type_t dtype, char *name, int target_ino){
    char tmp_blk[FS_BLOCK_SIZE];
    assert(inode_ptr->type == I_DIR);
    for (int index = 0;;index++){
        int sec_offset = fs_get_file_blk(index, inode_ptr);
        if (sec_offset == 0)    break;

        // check for emtpy entry
        fs_read_block(sec_offset, tmp_blk);
        dentry_t *dentry_ptr = (dentry_t *)tmp_blk;
        int ITE_BOUND = FS_BLOCK_SIZE / FS_DENTRY_SIZE;
        for (int i = 0; i < ITE_BOUND; i++){
            if (dentry_ptr[i].dtype == D_NULL){
                dentry_ptr[i].ino = target_ino;
                dentry_ptr[i].dtype = dtype;
                strcpy(dentry_ptr[i].name, name);
                fs_write_block(sec_offset, tmp_blk);
                return 1;
            }
        }
    }
    return 0;
}

// [p6] get the block sector by id
int fs_get_file_blk(int blk_index, inode_t *inode_ptr){
    if (blk_index < DIRECT_NUM)                     // Direct
        return inode_ptr->direct[blk_index];
    blk_index -= DIRECT_NUM;

    int base = FS_BLOCK_SIZE / sizeof(int);
    char tmp_blk[FS_BLOCK_SIZE];
    if (blk_index < INDIRECT1_NUM * base){          // Indirect1
        int index1 = blk_index / base;
        int index2 = blk_index % base;
        if (inode_ptr->indirect1[index1])
            fs_read_block(inode_ptr->indirect1[index1], tmp_blk);
        else
            return 0;
        return *((int *)tmp_blk +index2);
    }
    blk_index -= INDIRECT1_NUM * base;

    if (blk_index < INDIRECT2_NUM * base * base){       // Indirect2
        int index1 = blk_index / (base * base);
        int index2 = (blk_index % (base * base)) / base;
        int index3 = (blk_index % (base * base)) % base;
        if (inode_ptr->indirect2[index1] == 0)
            return 0;
        else {
            fs_read_block(inode_ptr->indirect2[index1], tmp_blk);
            if (*((int *)tmp_blk + index2) == 0)
                return 0;
            else {
                fs_read_block(*((int *)tmp_blk + index2), tmp_blk);
                return *((int *)tmp_blk + index3);
            }
        }
    }
    blk_index -= INDIRECT2_NUM * base * base;

    if (blk_index < INDIRECT3_NUM * base * base * base){    // Indirect3
        int index1 = blk_index / (base * base * base);
        int index2 = (blk_index % (base * base * base)) / (base * base);
        int index3 = (blk_index % (base * base * base)) % (base * base) / base;
        int index4 = (blk_index % (base * base * base)) % (base * base) % base;
        if (inode_ptr->indirect3[index1] == 0)
            return 0;
        else {
            fs_read_block(inode_ptr->indirect3[index1], tmp_blk);
            if (*((int *)tmp_blk + index2) == 0)
                return 0;
            else {
                fs_read_block(*((int *)tmp_blk + index2), tmp_blk);
                if (*((int *)tmp_blk + index3) == 0)
                    return 0;
                else {
                    fs_read_block(*((int *)tmp_blk + index3), tmp_blk);
                    return *((int *)tmp_blk + index4);
                }
            }
        }
    }
    return 0;
}

// [p6] Make File System
void do_mkfs(int force){
    printk("[FS] Checking Existence ...\n");
    fs_read_superblock();
    if (SuperBlock.magic != FS_SUPER_MAGIC || force){
        // Setting Super Block
        printk("[FS] Setting superblock...\n");
        SuperBlock.magic = FS_SUPER_MAGIC;
        SuperBlock.fs_start_sector = FS_SEC_START;
        SuperBlock.bbitmap_start = FS_DATAMAP_OFFSET;
        SuperBlock.bbitmap_secNum = FS_DATAMAP_SECNUM;
        SuperBlock.ibitmap_start = FS_INODEMAP_OFFSET;
        SuperBlock.ibitmap_secNum = FS_INODEMAP_SECNUM;
        SuperBlock.inode_start = FS_INODE_OFFSET;
        SuperBlock.inode_secNum = FS_INODE_SECNUM;
        SuperBlock.data_start = FS_DATA_OFFSET;
        SuperBlock.data_secNum = FS_DATA_SECNUM;
        SuperBlock.inode_size = FS_INODE_SIZE;
        SuperBlock.dentry_size = FS_DENTRY_SIZE;
        fs_write_superblock();
        printk("    MAGIC: 0x%x\n", FS_SUPER_MAGIC);
        printk("    FS Start Sector: %d\n", FS_SEC_START);
        printk("    FS Size (Total Data Block Size): %d sectors (%d MB)\n", FS_DATA_SECNUM, FS_DATA_SECNUM / 1024 / 1024 * FS_SECTOR_SIZE);
        printk("    FS Inode Map offset: %d, Occupy: %d sectors (%d KB)\n", FS_INODEMAP_OFFSET, FS_INODEMAP_SECNUM, FS_INODEMAP_SECNUM * FS_SECTOR_SIZE / 1024);
        printk("    FS Data Map offset: %d, Occupy: %d sectors (%d KB)\n", FS_DATAMAP_OFFSET, FS_DATAMAP_SECNUM, FS_DATAMAP_SECNUM * FS_SECTOR_SIZE / 1024);
        printk("    FS Inode offset: %d, Occupy: %d sectors (%d KB)\n", FS_INODE_OFFSET, FS_INODE_SECNUM, FS_INODE_SECNUM * FS_SECTOR_SIZE / 1024);
        printk("    FS Data Block offset: %d\n", FS_DATA_OFFSET);
        printk("    FS Inode entry size: %d bytes, Dir entry size: %d bytes\n", FS_INODE_SIZE, FS_DENTRY_SIZE);

        void *buffer;
        int ITE_BOUND = FS_BLOCK_SIZE / sizeof(uint64_t);
        // Setting Inode bitmap
        printk("[FS] Setting inode bitmap ...\n");
        buffer = inode_bitmap_buf;
        for (int i = 0; i < ITE_BOUND; i++){
            *((uint64_t *)buffer) = 0;
            buffer += sizeof(uint64_t);
        }
        fs_write_inodemap();

        // Setting DataBlock bitmap
        printk("[FS] Setting data bitmap ...\n");
        buffer = data_bitmap_buf;
        for (int i = 0; i < ITE_BOUND; i++){
            *((uint64_t *)buffer) = 0;
            buffer += sizeof(uint64_t);
        }
        for (int id = 0; id < FS_DATAMAP_BLKNUM; id++){
            screen_move_cursor_c(-SCREEN_WIDTH, -1);
            printk("[FS] Setting data bitmap ...(%d/%d)\n", id + 1, FS_DATAMAP_BLKNUM);
            fs_write_datamap(id);
        }

        inode_t tmp_inode;
        // Setting inode
        /*printk("[FS] Setting inodes ...\n");
        for (int i = 0; i < FS_INODE_BLKNUM; i++){
            screen_move_cursor_c(-SCREEN_WIDTH, -1);
            printk("[FS] Setting inodes ...(%d/%d)\n", i, FS_INODE_BLKNUM);
            fs_clearBlk(FS_INODE_OFFSET + 8 * i);
        }*/

        // Create Root Dir
        printk("[FS] Setting root dir ...\n");
        int ino = alloc_inode();
        fs_read_inode(&tmp_inode, ino);

        // basic info
        tmp_inode.ino = ino;
        tmp_inode.create_time = tmp_inode.modify_time = get_timer();
        tmp_inode.pino = ino;
        tmp_inode.type = I_DIR;
        tmp_inode.access = O_RW;

        // allocate a data block
        fs_addBlk_ptr(&tmp_inode);
        tmp_inode.file_size = FS_BLOCK_SIZE;

        // hard-link ?
        fs_mk_dentry(&tmp_inode, D_DIR, ".", ino);
        fs_mk_dentry(&tmp_inode, D_DIR, "..", ino);
        tmp_inode.link_cnt = 0;

        // name
        strcpy(tmp_inode.name, "/");
        fs_write_inode(&tmp_inode, ino);

        printk("[FS] Initialization finish!\n");
    }
    else {
        printk("[FS] File System already exists, exiting ...\n");
    }
    return;
}

void do_statfs(){
    fs_read_superblock();
    if (SuperBlock.magic == FS_SUPER_MAGIC){
        printk("[FS] File System Status:\n");
        printk("    MAGIC: 0x%x\n", SuperBlock.magic);
        printk("    FS Start Sector: %d\n", SuperBlock.fs_start_sector);
        printk("    FS Inode entry size: %d bytes, Dir entry size: %d bytes\n", SuperBlock.inode_size, SuperBlock.dentry_size);

        // Calculate the use of inodes
        printk("    Checking inode usage...\n");
        int inomap_use = 0, inomap_total;
        fs_read_inodemap();
        for (int i = 0; i < FS_BLOCK_SIZE; i++){
            for (int j = 0; j < 8; j++)
                inomap_use += ((inode_bitmap_buf[i] & byte_bits[j]) != 0);
        }
        inomap_total = SuperBlock.inode_secNum * FS_SECTOR_SIZE * 8;
        screen_move_cursor_c(-SCREEN_WIDTH, -1);
        printk("    FS Inode Map offset: %d, Occupy: %d sectors\n", SuperBlock.ibitmap_start, SuperBlock.ibitmap_secNum);
        printk("    FS Inode offset: %d, Use: %d/%d\n", SuperBlock.inode_start, inomap_use, inomap_total);

        // Calculate the use of data block
        printk("    Checking data block usage...\n");
        int datablk_use = 0, datablk_total;
        for (int id = 0; id < FS_DATAMAP_BLKNUM; id++){
            screen_move_cursor_c(-SCREEN_WIDTH, -1);
            printk("    Checking data block usage...(%d/%d)\n", id + 1, FS_DATAMAP_BLKNUM);
            fs_read_datamap(id);
            for (int i = 0; i < FS_BLOCK_SIZE; i++){
                for (int j = 0; j < 8; j ++)
                    datablk_use += ((data_bitmap_buf[i] & byte_bits[j]) != 0);
            }
        }
        datablk_total = FS_DATAMAP_BLKNUM * FS_BLOCK_SIZE * 8;
        screen_move_cursor_c(-SCREEN_WIDTH, -1);
        printk("    FS Data Map offset: %d, Occupy: %d\n", SuperBlock.bbitmap_start, SuperBlock.bbitmap_secNum);
        printk("    FS Data Block offset: %d, Use: %d/%d\n", SuperBlock.data_start, datablk_use, datablk_total);
    }
    else{
        printk("[FS] Can't find file system! :(\n");
    }
    return;
}

char dir[FS_PATH_DEPTH][FS_NAME_LEN];
int dir_dep;
/* [p6] look up*/
int fs_dentry_lookup(char *name, int cur_ino, dentry_type_t dtype){
    inode_t tmp_inode;
    fs_read_inode(&tmp_inode, cur_ino);
    assert(tmp_inode.type == I_DIR);

    dentry_t tmp_dentry;
    tmp_dentry.dtype = dtype;
    strcpy(tmp_dentry.name, name);

    int rt_ino = -1;
    // Direct
    rt_ino = walkthrough_index(0, (int *)tmp_inode.direct, DIRECT_NUM, 0, 0, FS_WALK_QUERY, &tmp_dentry);
    if (rt_ino != -1)
        return rt_ino;

    // Indirect1
    rt_ino = walkthrough_index(1, (int *)tmp_inode.indirect1, INDIRECT1_NUM, 0, 0, FS_WALK_QUERY, &tmp_dentry);
    if (rt_ino != -1)
        return rt_ino;

    // Indirect2
    rt_ino = walkthrough_index(2, (int *)tmp_inode.indirect2, INDIRECT2_NUM, 0, 0, FS_WALK_QUERY, &tmp_dentry);
    if (rt_ino != -1)
        return rt_ino;
    
    // Indirect3
    rt_ino = walkthrough_index(3, (int *)tmp_inode.indirect3, INDIRECT3_NUM, 0, 0, FS_WALK_QUERY, &tmp_dentry);
    if (rt_ino != -1)
        return rt_ino;

    return rt_ino;
}

/* [p6] return the depth of path, for example, 'genshin/start' is depth 2, 'genshin' is depth 1, '/genshin' is depth 2*/
int path_parser(char *path){
    int len = strlen(path), dep = 0;
    for (int cur = 0, dir_cur; cur < len;){
        dir_cur = 0;
        while (path[cur] != '/' && cur < len){
            dir[dep][dir_cur] = path[cur];
            dir_cur++;
            cur++;
        }
        dir[dep][dir_cur] = '\0';
        cur++;
        dep++;
    }
    return dep;
}

// strip_last: for mkdir
int walk_path(char *path, int strip_last){
    int cpuid = get_current_cpu_id();
    int dep = path_parser(path);
    int cur_ino, i_start = 0;
    if (dir[0][0] == '\0'){                              // absolute
        cur_ino = 0;
        i_start = 1;
    }
    else
        cur_ino = current_running[cpuid]->pwd;        // relative
    
    dir_dep = dep;
    dep -= strip_last;
    for (int i = i_start; i < dep; i++){
        cur_ino = fs_dentry_lookup(dir[i], cur_ino, D_DIR);
        if (cur_ino == -1){
            printk("[FS] Error: unknown dir: %s\n", dir[i]);
            return -1;
        }
    }
    return cur_ino;
}

int do_cd(char *path){
    int cpuid = get_current_cpu_id();
    int cur_ino = walk_path(path, 0);
    if (cur_ino != -1)
        current_running[cpuid]->pwd = cur_ino;
    return cur_ino;
}

// [p6] ls (mode: -a(0b01), -l(0b10), -a & -l(0b11))
int do_ls(int mode, char *path){
    int cpuid = get_current_cpu_id();
    int cur_ino = (path == NULL) ? current_running[cpuid]->pwd : walk_path(path, 0);

    if (cur_ino != -1){
        inode_t tmp_inode;
        fs_read_inode(&tmp_inode, cur_ino);

        dentry_t *dentry_ptr;
        char tmp_blk[FS_BLOCK_SIZE];
        int ITE_BOUND = FS_BLOCK_SIZE / FS_DENTRY_SIZE;
        for (int index = 0;;index++){
            int sec_offset = fs_get_file_blk(index, &tmp_inode);
            if (sec_offset == 0)    break;
            fs_read_block(sec_offset, tmp_blk);
            dentry_ptr = (dentry_t *)tmp_blk;
            for (int i = 0; i < ITE_BOUND; i++)
                if (dentry_ptr[i].dtype != D_NULL){
                    printk("%s ", dentry_ptr[i].name);
                }
        }
        printk("\n");
        return 1;
    }
    else{
        return 0;
    }
}

// [p6] mkdir
int do_mkdir(char *path){
    if (path == NULL){
        printk("[FS] Error: missing operand.\n");
        return 0;
    }

    int cur_ino = walk_path(path, 1);
    inode_t tmp_inode;
    fs_read_inode(&tmp_inode, cur_ino);
    dentry_t tmp_dentry;
    tmp_dentry.dtype = D_DIR;
    strcpy(tmp_dentry.name, dir[dir_dep - 1]);

    // check existence
    if (walkthrough_index(0, (int *)tmp_inode.direct, DIRECT_NUM, 0, 0, FS_WALK_QUERY, &tmp_dentry) != -1){
        printk("[FS] Error: dir %s already exists.\n", dir[dir_dep - 1]);
        return 0;
    }
    if (walkthrough_index(1, (int *)tmp_inode.indirect1, INDIRECT1_NUM, 0, 0, FS_WALK_QUERY, &tmp_dentry) != -1){
        printk("[FS] Error: dir %s already exists.\n", dir[dir_dep - 1]);
        return 0;
    }
    if (walkthrough_index(2, (int *)tmp_inode.indirect2, INDIRECT2_NUM, 0, 0, FS_WALK_QUERY, &tmp_dentry) != -1){
        printk("[FS] Error: dir %s already exists.\n", dir[dir_dep - 1]);
        return 0;
    }
    if (walkthrough_index(3, (int *)tmp_inode.indirect3, INDIRECT3_NUM, 0, 0, FS_WALK_QUERY, &tmp_dentry) != -1){
        printk("[FS] Error: dir %s already exists.\n", dir[dir_dep - 1]);
        return 0;
    }

    // insert
    int ino = fs_addFile(&tmp_inode, I_DIR, O_RW, dir[dir_dep - 1]);
    if (ino == -1){
        printk("[FS] Error: insufficient inodes.\n");
        return 0;
    }
    else{
        fs_mk_dentry(&tmp_inode, D_DIR, dir[dir_dep - 1], ino);
        tmp_inode.modify_time = get_timer();
        fs_write_inode(&tmp_inode, tmp_inode.ino);
    }
    return 1;
}

void print_pwd(int cur_ino){
    if (cur_ino == 0)
        return;
    else{
        inode_t tmp_inode;
        fs_read_inode(&tmp_inode, cur_ino);
        print_pwd(tmp_inode.pino);
        printk("/%s", tmp_inode.name);
    }
    return;
}

void do_pwd(){
    int cpuid = get_current_cpu_id();
    print_pwd(current_running[cpuid]->pwd);
    printk("\n");
    return;
}

void fs_release_inode(inode_t *inode_ptr){
    // release datablk
}

// [p6] rmdir
int fs_chk_dir_empty(int ino){
    inode_t tmp_inode;
    fs_read_inode(&tmp_inode, ino);
    assert(tmp_inode.type == I_DIR);

    // Check every dir entry
    char tmp_blk[FS_BLOCK_SIZE];
    for (int index = 0;;index++){
        int sec_offset = fs_get_file_blk(index, &tmp_inode);
        if (sec_offset == 0)    break;

        // check for emtpy entry
        fs_read_block(sec_offset, tmp_blk);
        dentry_t *dentry_ptr = (dentry_t *)tmp_blk;
        int ITE_BOUND = FS_BLOCK_SIZE / FS_DENTRY_SIZE;
        for (int i = 0; i < ITE_BOUND; i++){
            if (dentry_ptr[i].dtype == D_FILE)
                return 0;
            if (dentry_ptr[i].ino != ino && dentry_ptr[i].ino != tmp_inode.pino && dentry_ptr[i].dtype == D_DIR){
                //if (!fs_chk_dir_empty(dentry_ptr[i].ino))
                    return 0;
            }
        }
    }

    // emtpy
    return 1;
}

void fs_del_dir(int ino){
    inode_t tmp_inode;
    fs_read_inode(&tmp_inode, ino);
    assert(tmp_inode.type == I_DIR);

    // check children
    char tmp_blk[FS_BLOCK_SIZE];
    for (int index = 0;;index++){
        int sec_offset = fs_get_file_blk(index, &tmp_inode);
        if (sec_offset == 0)    break;

        // check for emtpy entry
        fs_read_block(sec_offset, tmp_blk);
        dentry_t *dentry_ptr = (dentry_t *)tmp_blk;
        int ITE_BOUND = FS_BLOCK_SIZE / FS_DENTRY_SIZE;
        for (int i = 0; i < ITE_BOUND; i++){
            if (dentry_ptr[i].dtype == D_FILE)
                assert(0);
            if (dentry_ptr[i].ino != ino && dentry_ptr[i].ino != tmp_inode.pino && dentry_ptr[i].dtype == D_DIR){
                fs_del_dir(dentry_ptr[i].ino);
            }
        }
    }

    // Release data block
    walkthrough_index(0, (int *)tmp_inode.direct, DIRECT_NUM, 0, 0, FS_WALK_DEL, NULL);
    walkthrough_index(1, (int *)tmp_inode.indirect1, INDIRECT1_NUM, 0, 0, FS_WALK_DEL, NULL);
    walkthrough_index(2, (int *)tmp_inode.indirect2, INDIRECT2_NUM, 0, 0, FS_WALK_DEL, NULL);
    walkthrough_index(3, (int *)tmp_inode.indirect3, INDIRECT3_NUM, 0, 0, FS_WALK_DEL, NULL);

    // Release inode
    release_inode(ino);
}

int do_rmdir(char *path){
    if (path == NULL){
        printk("[FS] Error: missing operand.\n");
        return 0;
    }

    int cur_ino = walk_path(path, 0);
    inode_t tmp_inode;
    fs_read_inode(&tmp_inode, cur_ino);
    if (tmp_inode.type != I_DIR){
        printk("[FS] Error: %s is not a directory.\n");
        return 0;
    }
    if (tmp_inode.ino == 0){
        printk("[FS] Panic: trying to delete root, aborting... \n");
        return 0;
    }

    // chk emtpy
    if (!fs_chk_dir_empty(cur_ino)){
        printk("[FS] Error: Directory not empty.\n");
        return 0;
    }

    // delte
    fs_del_dir(cur_ino);
    return 1;
}