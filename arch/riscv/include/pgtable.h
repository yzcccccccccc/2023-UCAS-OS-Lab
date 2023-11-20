#ifndef PGTABLE_H
#define PGTABLE_H

#include <type.h>

#define SATP_MODE_SV39 8
#define SATP_MODE_SV48 9

#define SATP_ASID_SHIFT 44lu
#define SATP_MODE_SHIFT 60lu

#define NORMAL_PAGE_SHIFT 12lu
#define NORMAL_PAGE_SIZE (1lu << NORMAL_PAGE_SHIFT)
#define LARGE_PAGE_SHIFT 21lu
#define LARGE_PAGE_SIZE (1lu << LARGE_PAGE_SHIFT)

/*
 * Flush entire local TLB.  'sfence.vma' implicitly fences with the instruction
 * cache as well, so a 'fence.i' is not necessary.
 */
static inline void local_flush_tlb_all(void)
{
    __asm__ __volatile__ ("sfence.vma" : : : "memory");
}

/* Flush one page from local TLB */
static inline void local_flush_tlb_page(unsigned long addr)
{
    __asm__ __volatile__ ("sfence.vma %0" : : "r" (addr) : "memory");
}

static inline void local_flush_icache_all(void)
{
    asm volatile ("fence.i" ::: "memory");
}

static inline void set_satp(
    unsigned mode, unsigned asid, unsigned long ppn)
{
    unsigned long __v =
        (unsigned long)(((unsigned long)mode << SATP_MODE_SHIFT) | ((unsigned long)asid << SATP_ASID_SHIFT) | ppn);
    __asm__ __volatile__("sfence.vma\ncsrw satp, %0" : : "rK"(__v) : "memory");
}

#define PGDIR_PA 0x51000000lu  // use 51000000 page as PGDIR

/*
 * PTE format:
 * | XLEN-1  10 | 9             8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0
 *       PFN      reserved for SW   D   A   G   U   X   W   R   V
 */

#define _PAGE_ACCESSED_OFFSET 6

#define _PAGE_PRESENT (1 << 0)
#define _PAGE_READ (1 << 1)     /* Readable */
#define _PAGE_WRITE (1 << 2)    /* Writable */
#define _PAGE_EXEC (1 << 3)     /* Executable */
#define _PAGE_USER (1 << 4)     /* User */
#define _PAGE_GLOBAL (1 << 5)   /* Global */
#define _PAGE_ACCESSED (1 << 6) /* Set by hardware on any access \
                                 */
#define _PAGE_DIRTY (1 << 7)    /* Set by hardware on any write */
#define _PAGE_SOFT (1 << 8)     /* Reserved for software */

#define _PAGE_PFN_SHIFT 10lu

#define VA_MASK ((1lu << 39) - 1)

#define PPN_BITS 9lu
#define NUM_PTE_ENTRY (1 << PPN_BITS)

typedef uint64_t PTE;

/* Translation between physical addr and kernel virtual addr */
static inline uintptr_t kva2pa(uintptr_t kva)
{
    /* TODO: [P4-task1] */
    return kva - 0xffffffc000000000lu;
}

static inline uintptr_t pa2kva(uintptr_t pa)
{
    /* TODO: [P4-task1] */
    return pa + 0xffffffc000000000lu;
}

/* get physical page addr from PTE 'entry' */
static inline uint64_t get_pa(PTE entry)
{
    /* TODO: [P4-task1] */
    return (entry >> _PAGE_PFN_SHIFT) << NORMAL_PAGE_SHIFT;
}

/* Get/Set page frame number of the `entry` */
static inline long get_pfn(PTE entry)
{
    /* TODO: [P4-task1] */
    return entry >> _PAGE_PFN_SHIFT;
}
static inline void set_pfn(PTE *entry, uint64_t pfn)
{
    /* TODO: [P4-task1] */
    *entry = ((*entry) & ((1lu << _PAGE_PFN_SHIFT) - 1))                // protect attributes bits and clear PFN bits in entry
            | (pfn << _PAGE_PFN_SHIFT);                                 // set PFN bits in entry
}

/* Get/Set attribute(s) of the `entry` */
static inline long get_attribute(PTE entry, uint64_t mask)
{
    /* TODO: [P4-task1] */
    return entry & 0xfflu & mask;
}
static inline void set_attribute(PTE *entry, uint64_t bits)
{
    /* TODO: [P4-task1] */
    *entry = ((*entry) & (~0xfflu))         // protect PFN bits and clear attributes bits
            | bits;                         // set attributes bits
}

// [p4-task1] this func is for setting the pgtab mapped memory cells to zero.
static inline void clear_pgdir(uintptr_t pgdir_addr)
{
    /* TODO: [P4-task1] */
    int step    = sizeof(int);
    int ite     = NORMAL_PAGE_SIZE / step;
    for (int i = 0; i < ite; i++){
        *((int *)pgdir_addr) = 0;
        pgdir_addr += step;
    }
}

// [p4] get the virtual frame of certain va (maybe 4KB alignment?)
static inline uint64_t get_vf(uint64_t va){
    // just clear the offset bits....
    return ((va & VA_MASK) >> NORMAL_PAGE_SHIFT) << NORMAL_PAGE_SHIFT;
}

#endif  // PGTABLE_H
