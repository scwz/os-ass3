#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <synch.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>

/* Place your page table functions here */
struct page_table_entry **page_table = NULL;
static size_t hpt_size = 0;

static struct lock *pt_lock = NULL;

static uint32_t
hpt_hash(struct addrspace *as, vaddr_t faultaddr) 
{
        uint32_t index;

        index = (((uint32_t) as) ^ (faultaddr >> PAGE_BITS)) % hpt_size;
        return index;
}

void page_table_init(void) 
{
        size_t i;

        for(i = 0; i < hpt_size; i++) {
                page_table[i] = NULL;
        }
}

void page_table_insert(void) 
{
        struct addrspace *as;
        as = 0;
        vaddr_t faultaddr = 0;
        hpt_hash(as, faultaddr);
}

void page_table_get(void) 
{

}

void vm_bootstrap(void)
{
        size_t nframes;
        paddr_t top_of_ram = ram_getsize();

        nframes = top_of_ram / PAGE_SIZE;
        hpt_size = nframes * 2;

        page_table = kmalloc(hpt_size * sizeof(struct page_table_entry *));
        frame_table = kmalloc(nframes * sizeof(struct frame_table_entry));

        frame_table_init(nframes);
        page_table_init();

        pt_lock = lock_create("page_table_lock");
        if (pt_lock == NULL) {
                panic("vm: failed to create page table lock");
        }
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

