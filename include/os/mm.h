/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                                   Memory Management
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */
#ifndef MM_H
#define MM_H

#include <os/sched.h>
#include <os/list.h>
#include <os/kernel.h>
#include <type.h>
#include <pgtable.h>

#define MAP_KERNEL 1
#define MAP_USER 2
#define MEM_SIZE 32
#define PAGE_SIZE 4096 // 4K
#define INIT_KERNEL_STACK 0xffffffc052000000
#define FREEMEM_KERNEL (INIT_KERNEL_STACK+4*PAGE_SIZE)
#define SHM_PAGE_BASE   0x80000000                      // virtual addr of the base of shared memory pages
#define SHM_PAGE_BOUND  (SHM_PAGE_BASE + NUM_MAX_SHMPAGE * NORMAL_PAGE_SIZE)

/* Rounding; only works for n = power of two */
#define ROUND(a, n)     (((((uint64_t)(a))+(n)-1)) & ~((n)-1))
#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n)-1))

/* [p4] page-frame management */
#define NUM_MAX_PHYPAGE     128
#define NUM_MAX_SWPPAGE     1024
#define NUM_MAX_SHMPAGE     64
#define LIST_PGF_OFFSET     16
#define PCBLIST_PGF_OFFSET  32

extern int free_page_num, free_swp_page_num, free_shm_page_num;

typedef enum{
    PF_UNPINNED,
    PF_PINNED,
    PF_UNUSED
}pf_type_t;

/************************************************
    [p4]:
        for managing physical page frame.
    to some degrees, it's for managing the
    physical memory space.
*************************************************/
typedef struct phy_pg{
    uint64_t    kva;                            // to some degrees, it's the physical frame addr
    uint64_t    va;                             // virtual frame addr
    list_node_t list;                           // link in used_queue (pinned & unpinned)
    list_node_t pcb_list;                       // pcb_list is used for the frame list in every pcb (may be of use when recycling)
    pcb_t       *user_pcb;
    pid_t       user_pid;
    uint64_t    attribute;
    pf_type_t   type;                           // pinned, unpinned, unused
}phy_pg_t;
extern phy_pg_t pf[NUM_MAX_PHYPAGE];
extern list_head free_pf, pinned_used_pf, unpinned_used_pf;

/************************************************
    [p4] About swap:
        when start a new process, we load the
    pages into the swap area as a copy. When
    facing swap-situation, we directly load/store
    the page into the corresponding position on
    the disk.
*************************************************/

// [p4] for managing the pages on the disk
typedef struct swp_pg{
    uint64_t    start_sector;                   // start_sector in swap area on the disk
    uint64_t    va;                             // virtual frame addr
    list_node_t list;                           // link in swp_queue
    list_node_t pcb_list;
    pcb_t       *user_pcb;
    pid_t       user_pid;
    uint64_t    attribute;
}swp_pg_t;
extern swp_pg_t sf[NUM_MAX_SWPPAGE];
extern list_head free_sf, used_sf;
extern uint64_t swap_start_offset, swap_start_sector;

// [p4] for managing the shared memory pages
typedef enum{
    SHM_UNUSED,
    SHM_USED
}shm_stat_t;
typedef struct shm_pg{
    uint64_t kva;
    int key;
    int user_num;                               // how many users?
    shm_stat_t status;
    phy_pg_t *phy_page;
}shm_pg_t;
extern shm_pg_t shm_f[NUM_MAX_SHMPAGE];
extern int query_SHM_kva(uint64_t kva);

extern ptr_t allocPage(int numPage);
extern ptr_t allocPage_from_freePF(int type, pcb_t *pcb_ptr, uint64_t va, uint64_t attribute);
extern void init_page();
extern void recycle_pages(pcb_t *pcb_ptr);
extern void allocPage_from_freeSF(pcb_t *pcb_ptr, uint64_t va, uint64_t attribute);
extern void swap_in(swp_pg_t *in_page);
extern void swap_out(phy_pg_t *out_page);
extern void transfer_page_p2s(uint64_t phy_addr, uint64_t start_sector);
extern void transfer_page_s2p(uint64_t phy_addr, uint64_t start_sector);
extern swp_pg_t *query_swp_page(uint64_t va, pcb_t *pcb_ptr);
extern void unmap(uint64_t va, uint64_t pgdir);
//extern uint64_t map(uint64_t va, uint64_t kva, uint64_t pgdir);

// TODO [P4-task1] */
void freePage(ptr_t baseAddr);

// #define S_CORE
// NOTE: only need for S-core to alloc 2MB large page
#ifdef S_CORE
#define LARGE_PAGE_FREEMEM 0xffffffc056000000
#define USER_STACK_ADDR 0x400000
extern ptr_t allocLargePage(int numPage);
#else
// NOTE: A/C-core
#define USER_STACK_ADDR 0xf00010000
#define USER_STACK_PAGE_NUM 2
#define USER_CRITICAL_AREA_UPPER    (USER_STACK_ADDR - USER_STACK_PAGE_NUM * NORMAL_PAGE_SIZE)
#define USER_CRITICAL_AREA_LOWER    (USER_CRITICAL_AREA_UPPER - NORMAL_PAGE_SIZE)
#endif

// TODO [P4-task1] */
extern void* kmalloc(size_t size);
extern void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir);
extern uintptr_t alloc_page_helper(uintptr_t va, pcb_t *pcb_ptr, int type, uint64_t attribute);
extern void unmap_boot();

// TODO [P4-task4]: shm_page_get/dt */
uintptr_t shm_page_get(int key);
void shm_page_dt(uintptr_t addr);
void shm_page_recycle(shm_pg_t *shm_ptr);

//------------------------------------- Security Page Management -------------------------------------
/*************************************************************************************
    About the security page
        considering this situation: core 0 just swaped out a user page (usr_pg), and
    core 1 happens to trap due to a syscall instruction. Unfortunately, this syscall
    coincides to pass a pointer arg, and the content of the pointer is in the usr_pg.
    So after core 0 unlcoks the kernel, core 1 enters the kernel and try to parse the
    content of the pointer, then a page-fault happens.
    
    2023.11.23
        to sovle this situation, I try to use a special page, namely 'security page',
    which is allocated to user at uva 0x5000 when create a process. we use this
    pinned page to pass the pointer args' content.
        (abandoned)

    2023.11.24
        fucking complex when encountering threads... (sevral threads may share the 
    same security page, leading to a sync problem). So I decide to put the security
    page in the kernel. First, allocate a unrecycable page (allocPage) for temporarity
    save the syscall args. Let's call it the 'arg_page'.  When user triggers a syscall 
    and traps into the kernel, check the presence of the args. If not, load the page
    into the kernel security page, copy the data from the security page to arg_page.

    2023.11.24
        fuck. I give up.... too many situations(what if part of the string in page A,
    while the rest in page B blablabla...). Finally I decide to use a mutex lock to
    protect the security page...
**************************************************************************************/

#define SECURITY_BASE   0x5000
#define SECURITY_BOUND  0x5f00

#endif /* MM_H */
