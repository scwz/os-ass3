#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>

/* Place your page table functions here */
struct page_table_entry **page_table = NULL;

void page_table_init(size_t npages) {
        (void) npages;
}

void vm_bootstrap(void)
{
        size_t nframes, npages;
        paddr_t top_of_ram = ram_getsize();

        nframes = top_of_ram / PAGE_SIZE;
        npages = nframes * 2;

        page_table = kmalloc(npages * sizeof(struct page_table_entry));
        frame_table = kmalloc(nframes * sizeof(struct frame_table_entry));
        frame_table_init(nframes);
        page_table_init(npages);
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
        (void) faulttype;
        (void) faultaddress;

        panic("vm_fault hasn't been written yet\n");

        return EFAULT;
}

/*
 *
 * SMP-specific functions.  Unused in our configuration.
 */

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
        (void)ts;
        panic("vm tried to do tlb shootdown?!\n");
}

