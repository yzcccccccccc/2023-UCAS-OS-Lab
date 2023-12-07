#include <os/ioremap.h>
#include <os/mm.h>
#include <pgtable.h>
#include <type.h>

// maybe you can map it to IO_ADDR_START ?
static uintptr_t io_base = IO_ADDR_START;

#define ALIGN_4KB   ~0xfff

void *ioremap(unsigned long phys_addr, unsigned long size)
{
    // TODO: [p5-task1] map one specific physical region to virtual address
    uint64_t start_phy_addr = phys_addr & ALIGN_4KB;
    uint64_t end_phy_addr   = phys_addr + size;

    // Step0: preparation!
    uint64_t pgdir = pa2kva(PGDIR_PA);
    uint64_t va, kva, rtv;
    uint64_t attribute = _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY;

    // Step1: start map!
    rtv = io_base + phys_addr - start_phy_addr;
    for (uint64_t phy_addr = start_phy_addr; phy_addr < end_phy_addr; 
            phy_addr += NORMAL_PAGE_SIZE, io_base += NORMAL_PAGE_SIZE){
        va  = io_base & VA_MASK;
        kva = pa2kva(phy_addr);
        map(va, kva, pgdir, attribute);
    }

    // Step2: flush tlb
    local_flush_tlb_all();
    
    return (void *)rtv;
}

void iounmap(void *io_addr)
{
    // TODO: [p5-task1] a very naive iounmap() is OK
    // maybe no one would call this function?
}
