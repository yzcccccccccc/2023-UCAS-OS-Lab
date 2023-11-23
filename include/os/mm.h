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

/* Rounding; only works for n = power of two */
#define ROUND(a, n)     (((((uint64_t)(a))+(n)-1)) & ~((n)-1))
#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n)-1))

/* [p4] page-frame management */
#define NUM_MAX_PHYPAGE     5
#define NUM_MAX_SWPPAGE     1024
#define LIST_PGF_OFFSET     16
#define PCBLIST_PGF_OFFSET  32

extern int free_page_num, free_swp_page_num;

typedef enum{
    UNPINNED,
    PINNED
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
}swp_pg_t;
extern swp_pg_t sf[NUM_MAX_SWPPAGE];
extern list_head free_sf, used_sf;
extern uint64_t swap_start_offset, swap_start_sector;

extern ptr_t allocPage(int numPage);
extern ptr_t allocPage_from_freePF(int type, pcb_t *pcb_ptr, uint64_t va);
extern void init_page();
extern void recycle_pages(pcb_t *pcb_ptr);
extern void allocPage_from_freeSF(pcb_t *pcb_ptr, uint64_t va);
extern void swap_in(swp_pg_t *in_page);
extern void swap_out(phy_pg_t *out_page);
extern void transfer_page_p2s(uint64_t phy_addr, uint64_t start_sector);
extern void transfer_page_s2p(uint64_t phy_addr, uint64_t start_sector);
extern swp_pg_t *query_swp_page(uint64_t va, pcb_t *pcb_ptr);
extern void unmap(uint64_t va, uint64_t pgdir);

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
extern uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir, pcb_t *pcb_ptr);
extern void unmap_boot();

// TODO [P4-task4]: shm_page_get/dt */
uintptr_t shm_page_get(int key);
void shm_page_dt(uintptr_t addr);



#endif /* MM_H */
