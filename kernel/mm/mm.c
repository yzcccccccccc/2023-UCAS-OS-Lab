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

/* [p4] */
pgf_t pf[NUM_MAX_PGFRAME];
LIST_HEAD(free_pf);
LIST_HEAD(pinned_used_pf);
LIST_HEAD(unpinned_used_pf);

// copy kernel pgtable into dest_pgdir (kva)
void copy_ker_pgdir(uint64_t dest_pgdir){
    PTE *dest_pgtab = (PTE *)dest_pgdir;
    PTE *ker_pgtab  = (PTE *)pa2kva(PGDIR_PA);
    int ITE_BOUND = NORMAL_PAGE_SIZE / sizeof(PTE);
    for (int i = 0; i < ITE_BOUND; i++)
        dest_pgtab[i] = ker_pgtab[i];
    return;
}

/****************************************************************
    In this case, we only use the pages in pf[]. In init_page(),
we need to allocate a kernel page (to some degrees, it's also the
physical page) for every item of pf[].
*****************************************************************/
void init_page(){
    for (int i = 0; i < NUM_MAX_PGFRAME; i++){
        pf[i].kva = allocPage(1);
        pf[i].user_pid = -1;                    // -1 means unallocated
        list_insert(&free_pf, &pf[i].list);
    }
}
// allocate 1 page from free_pf (list) for pcb, return kva of the page, type  PINNED or UNPINNED
ptr_t allocPage_from_freePF(int type, pcb_t *pcb_ptr, uint64_t va){
    list_node_t *pf_list_ptr;
    pgf_t *pf_ptr = NULL;
    if (!list_empty(&free_pf)){
        pf_list_ptr = list_pop(&free_pf);
        pf_ptr = (pgf_t *)((void *)pf_list_ptr - LIST_PGF_OFFSET);
        if (type == PINNED)
            list_insert(&pinned_used_pf, pf_list_ptr);
        else
            list_insert(&unpinned_used_pf, pf_list_ptr);
        list_insert(&pcb_ptr->pf_list, &(pf_ptr->pcb_list));               // upload to certain pcb!
        pf_ptr->va          = get_vf(va);
        pf_ptr->user_pid    = pcb_ptr->pid;
    }
    return pf_ptr == NULL ? 0 : pf_ptr->kva;
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
void unmap_boot(){
    PTE *pgdir = (PTE *)pa2kva(PGDIR_PA), *pmd1;
    uint64_t vpn2, vpn1;
    for (uint64_t va = 0x50000000lu; va < 0x51000000lu; va += 0x200000lu){
        vpn2 = (va & VA_MASK) >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
        vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
        pmd1 = (PTE *)pa2kva(get_pa(pgdir[vpn2]));
        pmd1[vpn1] = 0;
    }
    return;
}

void freePage(ptr_t baseAddr)
{
    // TODO [P4-task1] (design you 'freePage' here if you need):
}

void *kmalloc(size_t size)
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):
    return NULL;
    
}


/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // TODO [P4-task1] share_pgtable:
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
/*************************************************************************************************
    [p4]
    map va (allocate a physical pageframe) into pagetable pgdir, and this page belongs to pcb_ptr
**************************************************************************************************/
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir, pcb_t *pcb_ptr)
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
        /***********************************************************************
                allocate a new page as a second level page-table. May be reused
            afterwards, so directly allocate by allocPage()
        ************************************************************************/
        set_pfn(&pmd2[vpn2], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd2[vpn2], _PAGE_PRESENT | _PAGE_USER);
        clear_pgdir(pa2kva(get_pa(pmd2[vpn2])));
    }
    PTE *pmd1 = (PTE *)pa2kva(get_pa(pmd2[vpn2]));
    if (!(pmd1[vpn1] & _PAGE_PRESENT)){
        // alloc a new page for the third level pagetable
        set_pfn(&pmd1[vpn1], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd1[vpn1], _PAGE_PRESENT | _PAGE_USER);
        clear_pgdir(pa2kva(get_pa(pmd1[vpn1])));
    }
    PTE *pmd0 = (PTE *)pa2kva(get_pa(pmd1[vpn1]));
    if (!(pmd0[vpn0] & _PAGE_PRESENT)){
        /********************************************************************
                allocate a real physical page for the va, thus it should be 
            unpinned.
        *********************************************************************/
        set_pfn(&pmd0[vpn0], kva2pa(allocPage_from_freePF(UNPINNED, pcb_ptr, va)) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd0[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY | _PAGE_USER);
    }
    return pa2kva(get_pa(pmd0[vpn0]));
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
    return 0;
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
}