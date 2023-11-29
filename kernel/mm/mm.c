#include <os/mm.h>
#include <os/smp.h>
#include <printk.h>
#include <os/time.h>
#include <assert.h>

#define PAGE_OCC_SEC    8           // 4KB = 8 * 512B

int free_page_num       = NUM_MAX_PHYPAGE;
int free_swp_page_num   = NUM_MAX_SWPPAGE;
int free_shm_page_num   = NUM_MAX_SHMPAGE;

// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;

ptr_t allocPage(int numPage)
{
    // align PAGE_SIZE
    ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
    kernMemCurr = ret + numPage * PAGE_SIZE;
    return ret;
}

/* [p4] manage the physical pages */
phy_pg_t pf[NUM_MAX_PHYPAGE];
LIST_HEAD(free_pf);
LIST_HEAD(pinned_used_pf);
LIST_HEAD(unpinned_used_pf);

/* [p4] manage the swap pages */
swp_pg_t sf[NUM_MAX_SWPPAGE];
LIST_HEAD(free_sf);
LIST_HEAD(used_sf);
uint64_t swap_start_offset = 0, swap_start_sector = 0;

/* [p4] manage the shared memory pages */
shm_pg_t shm_f[NUM_MAX_SHMPAGE];
LIST_HEAD(free_shm_f);
LIST_HEAD(used_shm_f);

/****************************************************************
    In this case, we only use the pages in pf[]. In init_page(),
we need to allocate a kernel page (to some degrees, it's also the
physical page) for every item of pf[].
    [p4] for swap:
    init swap pages
*****************************************************************/
void init_page(){
    // physical page
    for (int i = 0; i < NUM_MAX_PHYPAGE; i++){
        pf[i].kva       = allocPage(1);
        pf[i].user_pid  = -1;                    // -1 means unallocated
        pf[i].type      = PF_UNUSED;
        list_insert(&free_pf, &pf[i].list);
    }

    // swap page
    uint64_t _sector = swap_start_sector;
    for (int i = 0; i < NUM_MAX_SWPPAGE; i++, _sector += PAGE_OCC_SEC){
        sf[i].start_sector  = _sector;
        sf[i].user_pid      = -1;
        list_insert(&free_sf, &sf[i].list);
    }

    // shared pages
    for (int i = 0; i < NUM_MAX_SHMPAGE; i++){
        shm_f[i].status = SHM_UNUSED;
        shm_f[i].user_num = 0;
    }
}

//------------------------------------- Physical Page Management -------------------------------------

// [p4] allocate 1 page from free_pf (list) for pcb, return kva of the page, type  PINNED or UNPINNED
ptr_t allocPage_from_freePF(int type, pcb_t *pcb_ptr, uint64_t va, uint64_t attribute){
    list_node_t *pf_list_ptr;
    phy_pg_t *pf_ptr = NULL;

retry:
    if (!list_empty(&free_pf)){                 // have free physical frame
        pf_list_ptr = list_pop(&free_pf);
        pf_ptr = (phy_pg_t *)((void *)pf_list_ptr - LIST_PGF_OFFSET);
        
        // insert into used_queue
        if (type == PF_PINNED)
            list_insert(&pinned_used_pf, pf_list_ptr);
        else
            list_insert(&unpinned_used_pf, pf_list_ptr);

        // load to given pcb
        list_insert(&pcb_ptr->pf_list, &(pf_ptr->pcb_list));
        
        // fill the info
        pf_ptr->va          = get_vf(va & VA_MASK);
        pf_ptr->user_pcb    = pcb_ptr;
        pf_ptr->user_pid    = pcb_ptr->pid;
        pf_ptr->type        = type;
        pf_ptr->attribute   = attribute;

        free_page_num--;
    }
    else{                                       // swap out a physical frame
        pf_list_ptr = list_pop(&unpinned_used_pf);
        pf_ptr = (phy_pg_t *)((void *)pf_list_ptr - LIST_PGF_OFFSET);
        swap_out(pf_ptr);
        goto retry;
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


/******************************************************************
    Unmap & recycle:
    ---------       ---------       ---------       ---------
    +       +       +       +       +       +       +       +
    +       +       +       +       +       +       +       +
    + vpn 2 +  ->   + vpn 1 + ->    + vpn 0 + ->    +       +
    +       +       +       +       +       +       +       +
    +       +       +       +       +       +       +       +
    +       +       +       +       +       +       +       +
    ---------       ---------       ---------       ---------
    First(pmd2)     Second(pmd1)    Third(pmd0)     physical page
    
    Unmap:      set pmd0[vpn0] to zero
    Recycle:    recycle the physical page
*******************************************************************/

// [p4] recycle the pages occupied by given pcb (both physical and swap)
void recycle_pages(pcb_t *pcb_ptr){
    list_node_t *ptr;

    // First: recycle physical page
    phy_pg_t *pf_node;
    uint64_t pgdir = pcb_ptr->pgdir;
    while (!list_empty(&pcb_ptr->pf_list)){
        ptr = list_pop(&pcb_ptr->pf_list);
        pf_node = (phy_pg_t *)((void *)ptr -PCBLIST_PGF_OFFSET);
        free_page_num++;

        // unmap
        unmap(pf_node->va & VA_MASK, pgdir);

        // check shm (va in shm?)
        if (pf_node->va >= SHM_PAGE_BASE && pf_node->va < SHM_PAGE_BOUND)
            shm_f[(pf_node->va - SHM_PAGE_BASE) / NORMAL_PAGE_SIZE].status = SHM_UNUSED;         // release

        pf_node->va = 0;
        pf_node->user_pid = -1;
        pf_node->user_pcb = NULL;
        pf_node->type     = PF_UNUSED;
        
        // remove from the used_queue & insert free_pf queue
        list_delete(&pf_node->list);
        list_insert(&free_pf, &pf_node->list);
    }

    // Second: recycle swap page
    swp_pg_t *sf_node;
    while (!list_empty(&pcb_ptr->sf_list)){
        ptr = list_pop(&pcb_ptr->sf_list);
        sf_node = (swp_pg_t *)((void *)ptr - PCBLIST_PGF_OFFSET);

        sf_node->va = 0;
        sf_node->user_pid = -1;
        sf_node->user_pcb = NULL;

        // remove from the used_queue and insert free_sf queue
        list_delete(&sf_node->list);
        list_insert(&free_sf, &sf_node->list);
        free_swp_page_num++;
    }

    // Third: recycle shm page
    uint64_t _va = SHM_PAGE_BASE, _kva;
    for (int i = 0, idx; i < NUM_MAX_SHMPAGE; i++, _va += NORMAL_PAGE_SIZE){
        if (pcb_ptr->shm_info & (1 << i)){
            pcb_ptr->shm_info &= (~(1 << i));
            _kva = get_kva_v(_va, pcb_ptr->pgdir);
            if (!_kva)
                continue;                   // continue if this is not a shm_Page

            // get the location in shm_f
            idx = query_SHM_kva(_kva);
            
            // recycle work
            shm_f[idx].user_num--;
            unmap(_va, pcb_ptr->pgdir);
            if (shm_f[idx].user_num == 0)
                shm_page_recycle(&shm_f[idx]);
        }
    }
    return;
}

// [p4-task1] unmap 0x50200000 ~ 0x51000000
void unmap_boot(){
    PTE *pgdir = (PTE *)pa2kva(PGDIR_PA), *pmd1;
    uint64_t vpn2, vpn1;
    for (uint64_t va = 0x50200000lu; va < 0x51000000lu; va += 0x200000lu){
        vpn2 = (va & VA_MASK) >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
        vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
        pmd1 = (PTE *)pa2kva(get_pa(pgdir[vpn2]));
        pmd1[vpn1] = 0;
    }
    return;
}

// [p4-task1] unmap the virtual frame (va) in the page-table
void unmap(uint64_t va, uint64_t pgdir){
    va &= VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (vpn2 << (2 * PPN_BITS)) ^
                    (vpn1 << PPN_BITS) ^
                    (va >> (NORMAL_PAGE_SHIFT));
    PTE *pmd2 = (PTE *)pgdir;
    if (!pmd2[vpn2])    return;
    PTE *pmd1 = (PTE *)pa2kva(get_pa(pmd2[vpn2]));
    if (!pmd1[vpn1])    return;
    PTE *pmd0 = (PTE *)pa2kva(get_pa(pmd1[vpn1]));
    pmd0[vpn0] = 0;
    return;
}

void map(uint64_t va, uint64_t kva, uint64_t pgdir, uint64_t attribute){
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (vpn2 << (2 * PPN_BITS)) ^
                    (vpn1 << PPN_BITS) ^
                    (va >> (NORMAL_PAGE_SHIFT));
    PTE *pmd2 = (PTE *)pgdir;
    if (!(pmd2[vpn2] & _PAGE_PRESENT)){
        set_pfn(&pmd2[vpn2], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd2[vpn2], _PAGE_PRESENT | _PAGE_USER);
        clear_pgdir(pa2kva(get_pa(pmd2[vpn2])));
    }
    PTE *pmd1 = (PTE *)pa2kva(get_pa(pmd2[vpn2]));
    if (!(pmd1[vpn1] & _PAGE_PRESENT)){
        set_pfn(&pmd1[vpn1], kva2pa(allocPage(1)) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd1[vpn1], _PAGE_PRESENT | _PAGE_USER);
        clear_pgdir(pa2kva(get_pa(pmd1[vpn1])));
    }
    PTE *pmd0 = (PTE *)pa2kva(get_pa(pmd1[vpn1]));
    set_pfn(&pmd0[vpn0], kva2pa(kva) >> NORMAL_PAGE_SHIFT);
    set_attribute(&pmd0[vpn0], _PAGE_PRESENT | _PAGE_USER | attribute);
}

void freePage(ptr_t baseAddr)
{
    // TODO [P4-task1] (design your 'freePage' here if you need):
}

void *kmalloc(size_t size)
{
    // TODO [P4-task1] (design your 'kmalloc' here if you need):
    return NULL;
    
}

/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // TODO [P4-task1] share_pgtable:
    PTE *dest_pgtab = (PTE *)dest_pgdir;
    PTE *src_pgtab  = (PTE *)src_pgdir;
    int ITE_BOUND = NORMAL_PAGE_SIZE / sizeof(PTE);
    for (int i = 0; i < ITE_BOUND; i++)
        dest_pgtab[i] = src_pgtab[i];
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
/*************************************************************************************************
    [p4]
    map va (allocate a physical pageframe) into pagetable pgdir, and this page belongs to pcb_ptr
**************************************************************************************************/
uintptr_t alloc_page_helper(uintptr_t va, pcb_t *pcb_ptr, int type, uint64_t attribute)
{
    // TODO [P4-task1] alloc_page_helper:
    va &= VA_MASK;
    uint64_t pgdir = pcb_ptr->pgdir;

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
                allocate a real physical page for the va
        *********************************************************************/
        set_pfn(&pmd0[vpn0], kva2pa(allocPage_from_freePF(type, pcb_ptr, va, attribute)) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd0[vpn0], _PAGE_PRESENT | _PAGE_USER | attribute);
        clear_pgdir(pa2kva(get_pa(pmd0[vpn0])));
    }
    return pa2kva(get_pa(pmd0[vpn0]));
}

//------------------------------------- Swap Page Management -------------------------------------

// [p4-task3] copy a page's content from phyical memory to swap area
void transfer_page_p2s(uint64_t phy_addr, uint64_t start_sector){
    bios_sd_write((unsigned int)phy_addr, PAGE_OCC_SEC, (unsigned int)start_sector);
}

// [p4-task3] copy a page's content from swap area to physical memory
void transfer_page_s2p(uint64_t phy_addr, uint64_t start_sector){
    bios_sd_read((unsigned int)phy_addr, PAGE_OCC_SEC, (unsigned int)start_sector);
}

// [p4-task3] alloc a page on the disk for virtual frame va
void allocPage_from_freeSF(pcb_t *pcb_ptr, uint64_t va, uint64_t attribute){
    va = get_vf(va & VA_MASK);
    list_node_t *sf_list_ptr = list_pop(&free_sf);
    swp_pg_t *sf_node = (swp_pg_t *)((void *)sf_list_ptr - LIST_PGF_OFFSET);
    free_swp_page_num--;

    sf_node->va = va;
    sf_node->user_pcb = pcb_ptr;
    sf_node->user_pid = pcb_ptr->pid;
    sf_node->attribute = attribute;
    list_insert(&(pcb_ptr->sf_list), &(sf_node->pcb_list));
    list_insert(&(used_sf), &(sf_node->list));

    // transfer!
    uint64_t phy_addr = kva2pa(get_kva_v(va, pcb_ptr->pgdir));
    transfer_page_p2s(phy_addr, sf_node->start_sector);
}

// [p4-task3] swap in the swap-area page
void swap_in(swp_pg_t *in_page){
    // debug
    //printl("[ticks: %ld][core %d] swap in page (va: %lx, pid: %d)\n", get_ticks(), get_current_cpu_id(), in_page->va, in_page->user_pcb->pid);

    // First: find a available physical page
    pcb_t *pcb_ptr = in_page->user_pcb;
    uint64_t va = in_page->va;
    uint64_t attribute = in_page->attribute;
    uint64_t alloc_kva = alloc_page_helper(va, pcb_ptr, PF_UNPINNED, attribute);

    // Second: transfer!
    uint64_t phy_addr = kva2pa(alloc_kva);
    transfer_page_s2p(phy_addr, in_page->start_sector);
}

// [p4-task3] swap out a physical page (write_back = 1: dirty, write to the swap area)
void swap_out(phy_pg_t *out_page){
    uint64_t va = out_page->va & VA_MASK;
    uint64_t pgdir = out_page->user_pcb->pgdir;

    pcb_t *pcb_ptr = out_page->user_pcb;

    // Step0: inform other core
    int other_cpu = get_current_cpu_id() ^ 1;
    if (current_running[other_cpu] == out_page->user_pcb)
        send_ipi(&cpu_mask_arr[other_cpu]);

    // Step1: write back (optional)
    bool write_back = get_attribute(get_PTE_va(va, pgdir), _PAGE_DIRTY);
    if (write_back){
        swp_pg_t *swp_page = query_swp_page(va, pcb_ptr);
        if (swp_page == NULL){                                          // not the thread stack page
            pcb_ptr = pcb_ptr->par;
            swp_page = query_swp_page(va, pcb_ptr);
        }
        uint64_t phy_addr = kva2pa(out_page->kva);
        transfer_page_p2s(phy_addr, swp_page->start_sector);
    }

    // Step2: update physical frame info and page-table
    unmap(va, pgdir);         // remove from the user page-table
    out_page->user_pid = -1;
    out_page->user_pcb = NULL;
    out_page->va = 0;
    list_delete(&(out_page->pcb_list));
    list_insert(&free_pf, &(out_page->list));
    free_page_num++;
    return;
}

// [p4-task3] query the swp-page info (va: virtual frame addr)
swp_pg_t *query_swp_page(uint64_t va, pcb_t *pcb_ptr){
    swp_pg_t *rt_ptr = NULL;
    list_node_t *ptr = pcb_ptr->sf_list.next;
    while (ptr != &(pcb_ptr->sf_list)){
        rt_ptr = (swp_pg_t *)((void *)ptr - PCBLIST_PGF_OFFSET);
        if (rt_ptr->va == va)
            return rt_ptr;
        ptr = ptr->next;
    }
    rt_ptr = NULL;
    return rt_ptr;
}

//------------------------------------- Shared Page Management -------------------------------------

// pid0_core0_pcb controls all the shm pages :)

phy_pg_t *query_PFofSHM(uint64_t va){
    list_node_t *list_ptr = pid0_core0_pcb.pf_list.next;
    phy_pg_t *phy_ptr = NULL;
    while (list_ptr != &pid0_core0_pcb.pf_list){
        phy_ptr = (phy_pg_t *)((void *)list_ptr - PCBLIST_PGF_OFFSET);
        if (phy_ptr->va == va)
            return phy_ptr;
        list_ptr = list_ptr->next;
    }
    return phy_ptr;
}

// [p4-task5] return the idx in shm_f[]
int query_SHM_kva(uint64_t kva){
    int rtidx = -1;
    for (int i = 0; i < NUM_MAX_SHMPAGE; i++){
        if (shm_f[i].status == SHM_USED && shm_f[i].kva == kva){
            rtidx = i;
            break;
        }
    }
    return rtidx;
}

uintptr_t shm_page_get(int key)
{
    int cpuid = get_current_cpu_id();
    // TODO [P4-task4] shm_page_get:
    uint64_t rt_va = 0, attribute = _PAGE_WRITE | _PAGE_READ | _PAGE_ACCESSED | _PAGE_DIRTY;
    int shm_idx = -1;

    // Step0: Find a available va for the current process
    uint64_t shm_info = current_running[cpuid]->shm_info, test_bit = 1;
    for (int i = 0; i < NUM_MAX_SHMPAGE; i++, test_bit <<= 1){
        if (!(shm_info & test_bit)){
            shm_info |= test_bit;
            current_running[cpuid]->shm_info = shm_info;
            rt_va = SHM_PAGE_BASE + i * NORMAL_PAGE_SIZE;
            break;
        }
    }
    assert(rt_va != 0);

    // Step1: Check the shm_f[]
    uint64_t rt_kva = 0;
    for (int i = 0; i < NUM_MAX_SHMPAGE; i++){
        if (shm_f[i].status == SHM_USED && shm_f[i].key == key){
            rt_kva = shm_f[i].kva;
            shm_idx = i;
            break;
        }
        if (shm_idx == -1 && shm_f[i].status == SHM_UNUSED)
            shm_idx = i;
    }

    // Step2: do the map work
    if (rt_kva == 0){                           // need a new shmPage
        free_shm_page_num--;
        shm_f[shm_idx].status = SHM_USED;
        shm_f[shm_idx].user_num = 1;
        shm_f[shm_idx].key = key;
        shm_f[shm_idx].kva = rt_kva = allocPage_from_freePF(PF_PINNED, &pid0_core0_pcb, rt_va, attribute);
        shm_f[shm_idx].phy_page = query_PFofSHM(rt_va);
        map(rt_va, shm_f[shm_idx].kva, current_running[cpuid]->pgdir, attribute);
    }
    else{
        shm_f[shm_idx].user_num++;
        map(rt_va, rt_kva, current_running[cpuid]->pgdir, attribute);
    }
    return rt_va;
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
    int cpuid = get_current_cpu_id();
    uint64_t _kva = get_kva_v(addr, current_running[cpuid]->pgdir);
    int idx = (addr - SHM_PAGE_BASE) / NORMAL_PAGE_SIZE;
    current_running[cpuid]->shm_info &= (~(1 << idx));
    for (int i = 0; i < NUM_MAX_SHMPAGE; i++){
        if (shm_f[i].status == SHM_USED && shm_f[i].kva == _kva){
            shm_f[i].user_num--;
            unmap(addr, current_running[cpuid]->pgdir);
            if (shm_f[i].user_num == 0)
                shm_page_recycle(&shm_f[i]);
            break;
        }
    }
    return;
}

void shm_page_recycle(shm_pg_t *shm_ptr){
    free_shm_page_num++;
    // physical page
    phy_pg_t *phy_ptr = shm_ptr->phy_page;
    list_delete(&phy_ptr->pcb_list);                 // delete from pid0_core0_pcb
    list_delete(&phy_ptr->list);                     // remove from used_quee
    list_insert(&free_pf, &phy_ptr->list);
    phy_ptr->type = PF_UNUSED;
    phy_ptr->user_pid = -1;
    phy_ptr->user_pcb = NULL;
    phy_ptr->va = 0;
    free_page_num++;

    // shm page info
    shm_ptr->status = SHM_UNUSED;
    shm_ptr->kva = 0;
    shm_ptr->user_num = 0;
    shm_ptr->phy_page = NULL;
    return;
}