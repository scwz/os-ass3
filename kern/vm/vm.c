#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <synch.h>
#include <proc.h>
#include <thread.h>
#include <current.h>
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

void 
page_table_init(void) 
{
        size_t i;

        for(i = 0; i < hpt_size; i++) {
                page_table[i] = NULL;
        }
}

void 
page_table_insert(struct addrspace *as, vaddr_t faultaddr) 
{
        struct page_table_entry *pt_entry = NULL;
        uint32_t hash = hpt_hash(as, faultaddr);

        pt_entry = kmalloc(sizeof(struct page_table_entry *));
        pt_entry->pid = (uint32_t) as;
        pt_entry->pfn = KVADDR_TO_PADDR(faultaddr);
        pt_entry->elo = faultaddr | TLB_VALID; 

        lock_acquire(pt_lock);

        pt_entry->next = page_table[hash];
        page_table[hash] = pt_entry;

        lock_release(pt_lock);
}

struct page_table_entry *
page_table_get(struct addrspace *as, vaddr_t faultaddr) 
{
        uint32_t pid;
        uint32_t hash;
        struct page_table_entry *pt_entry = NULL;

        pid = (uint32_t) as;
        hash = hpt_hash(as, faultaddr);

        lock_acquire(pt_lock);

        pt_entry = page_table[hash];
        for (; pt_entry->pid != pid && pt_entry != NULL; 
                        pt_entry = pt_entry->next);

        lock_release(pt_lock);

        return pt_entry;
}

void vm_bootstrap(void)
{
        unsigned int nframes;
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
        struct addrspace *as;
        (void) faultaddress;

        switch (faulttype) {
                case VM_FAULT_READONLY:
                        return EFAULT;
                case VM_FAULT_READ:
                case VM_FAULT_WRITE:
                        break;
                default:
                        return EINVAL;
        }

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = proc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

        panic("vm_fault hasn't been completed yet\n");

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

