#include <types.h>
#include <kern/errno.h>
#include <spl.h>
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

static struct lock *pt_lock;

static uint32_t
hpt_hash(struct addrspace *as, vaddr_t faultaddr) 
{
        uint32_t index;

        index = (((uint32_t) as) ^ (faultaddr >> PAGE_BITS)) % hpt_size;
        return index;
}

static void 
page_table_init(void) 
{
        for (size_t i = 0; i < hpt_size; i++) {
                page_table[i] = NULL;
        }
}

static struct page_table_entry *
page_table_insert(struct addrspace *as, vaddr_t faultaddr, int accmode) 
{
        vaddr_t vaddr;
        struct page_table_entry *pte;
        uint32_t hash = hpt_hash(as, faultaddr);

        pte = kmalloc(sizeof(struct page_table_entry));
        if (pte == NULL) {
                return NULL;
        }

        /* allocate a frame */
        vaddr = alloc_kpages(1);

        pte->pid = (uint32_t) as;
        pte->vpn = faultaddr;
        pte->elo = KVADDR_TO_PADDR(vaddr) | TLBLO_VALID; 
 
        if (accmode & REGION_W) {
                pte->elo |= TLBLO_DIRTY;
        }

        pte->next = page_table[hash];
        page_table[hash] = pte;

        return pte;
}

static struct page_table_entry *
page_table_get(struct addrspace *as, vaddr_t faultaddr) 
{
        uint32_t pid, hash;
        struct page_table_entry *curr;

        pid = (uint32_t) as;
        hash = hpt_hash(as, faultaddr);

        for (curr = page_table[hash]; curr != NULL; curr = curr->next) {
                if (curr->pid == pid && curr->vpn == faultaddr) {
                        return curr;
                }
        }

        return NULL;
}

static struct region *
region_get(struct addrspace *as, vaddr_t faultaddress)
{
        vaddr_t vtop;
        struct region *curr;

        for (curr = as->regions; curr != NULL; curr = curr->next) {
                vtop = curr->vbase + curr->size;
                if (faultaddress >= curr->vbase && faultaddress < vtop) {
                        return curr;
                }
        }

        return NULL;
}

int
page_table_copy(struct addrspace *oldas, struct addrspace *newas) 
{
        struct page_table_entry *curr, *new;

        for (size_t i = 0; i < hpt_size; i++) {
                lock_acquire(pt_lock);
                for(curr = page_table[i]; curr != NULL; curr = curr->next) {
                        if (curr->pid == (uint32_t) oldas) {
                                struct region *rgn = region_get(oldas, curr->vpn);
                                new = page_table_insert(newas, 
                                                        curr->vpn, 
                                                        rgn->accmode);
                                if (new == NULL) {
                                        return ENOMEM;
                                }

                                memmove((void *) PADDR_TO_KVADDR(new->elo & TLBLO_PPAGE),
                                        (void *) PADDR_TO_KVADDR(curr->elo & TLBLO_PPAGE),
                                        PAGE_SIZE);
                        }
                }
                lock_release(pt_lock); 
        }

        return 0;
}

void
page_table_remove(struct addrspace *as) 
{
        struct page_table_entry *curr, *next, *prev;

        for (size_t i = 0; i < hpt_size; i++) {
                lock_acquire(pt_lock);
                prev = NULL;
                for (curr = page_table[i]; curr != NULL; curr = next) {
                        next = curr->next;
                        if (curr->pid == (uint32_t) as) {
                                free_kpages(PADDR_TO_KVADDR(curr->elo & TLBLO_PPAGE));
                                kfree(curr);
                                if (prev != NULL) {
                                        prev->next = next;
                                }
                                else {
                                        page_table[i] = next;
                                }
                        }
                        else {
                                prev = curr;
                        }
                }
                lock_release(pt_lock); 
        }
}

void
page_table_load(struct addrspace *as, struct region *rgn, int load) 
{
        struct page_table_entry *curr;

        for (size_t i = 0; i < hpt_size; i++) {
                lock_acquire(pt_lock);
                for(curr = page_table[i]; curr != NULL; curr = curr->next) {
                        uint32_t pid = (uint32_t) as;
                        if (curr->pid == pid && curr->vpn == rgn->vbase) {
                                if (load) {
                                        curr->elo |= TLBLO_DIRTY;
                                }
                                else {
                                        if (!(rgn->accmode & REGION_W)) {
                                                curr->elo &= ~TLBLO_DIRTY;
                                        }
                                }
                        }
                }
                lock_release(pt_lock); 
        }
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
        int spl;
        struct addrspace *as;
        struct region *region;
        struct page_table_entry *pte;

        faultaddress &= PAGE_FRAME;

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

        lock_acquire(pt_lock);
        pte = page_table_get(as, faultaddress);
        lock_release(pt_lock);

        if (pte == NULL) {
                /* find valid region */
                region = region_get(as, faultaddress);

                if (region == NULL) {
                        return EFAULT;
                }

                /* insert into page table */
                lock_acquire(pt_lock);
                pte = page_table_insert(as, faultaddress, region->accmode);
                lock_release(pt_lock);

                if (pte == NULL) {
                        return ENOMEM;
                }
        }

        /* valid translation, write into tlb */
        spl = splhigh();
        tlb_random(faultaddress, pte->elo);
        splx(spl);

        return 0;
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

