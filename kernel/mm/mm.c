#include <os/mm.h>

// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;

ptr_t allocPage(int numPage)
{
    // align PAGE_SIZE
    ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
    kernMemCurr = ret + numPage * PAGE_SIZE;
    return ret;
}

// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage)
{
    // align LARGE_PAGE_SIZE
    ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
    largePageMemCurr = ret + numPage * LARGE_PAGE_SIZE;
    return ret;    
}
#endif

// [p4-task1] unmap 0x50200000 ~ 0x51000000
void unmap(){

}

void freePage(ptr_t baseAddr)
{
    // TODO [P4-task1] (design you 'freePage' here if you need):
}

void *kmalloc(size_t size)
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):
    
}


/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // TODO [P4-task1] share_pgtable:
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir)
{
    // TODO [P4-task1] alloc_page_helper:
    va &= VA_MASK;

    //--------------------------------get VPN bits--------------------------------
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (vpn2 << (2 * PPN_BITS)) ^
                    (vpn1 << PPN_BITS) ^
                    (va >> (NORMAL_PAGE_SHIFT));

    //--------------------------------aloc PTE--------------------------------
    PTE *pmd2 = (PTE *)pgdir;
    if (!(pmd2[vpn2] & _PAGE_PRESENT)){
        // alloc a new page
        set_pfn(&pmd2[vpn2], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd2[vpn2], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pmd2[vpn2])));
    }
    PTE *pmd1 = (PTE *)pa2kva(get_pa(pmd2[vpn2]));
    if (!(pmd1[vpn1] & _PAGE_PRESENT)){
        // alloc a new page
        set_pfn(&pmd1[vpn1], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd1[vpn1], _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(pmd1[vpn1])));
    }
    PTE *pmd0 = (PTE *)pa2kva(get_pa(pmd1[vpn1]));
    if (!(pmd0[vpn0] & _PAGE_PRESENT)){
        set_pfn(&pmd0[vpn0], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd0[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY);
    }
    return pa2kva(get_pa(pmd0[vpn0]));
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
}