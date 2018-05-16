#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>

/* Place your frametable data-structures here 
 * You probably also want to write a frametable initialisation
 * function and call it from vm_bootstrap
 */

struct frame_table_entry *frame_table = NULL;
static struct frame_table_entry *free_frame_ptr = NULL;

static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

void frame_table_init(size_t nframes) 
{
        size_t i;
        size_t firstfree;

        /* reserve space for the frame and page tables */
        for (i = 0; i < nframes; i++) {
                frame_table[i].next_free_frame = NULL;
        }

        /* make free frame list */
        firstfree = ram_getfirstfree() / PAGE_SIZE;
        for (i = firstfree; i < nframes - 1; i++) {
                frame_table[i].next_free_frame = &frame_table[i + 1];
        }

        free_frame_ptr = &frame_table[firstfree];
}

/* Note that this function returns a VIRTUAL address, not a physical 
 * address
 * WARNING: this function gets called very early, before
 * vm_bootstrap().  You may wish to modify main.c to call your
 * frame table initialisation function, or check to see if the
 * frame table has been initialised and call ram_stealmem() otherwise.
 */

vaddr_t alloc_kpages(unsigned int npages)
{
        paddr_t addr;

        if (frame_table == NULL) {
                spinlock_acquire(&stealmem_lock);
                addr = ram_stealmem(npages);
                spinlock_release(&stealmem_lock);

                if(addr == 0)
                        return 0;
        }
        else {
                KASSERT(npages == 1);   // remove this line later

                /* only allocate 1 page */
                if (npages != 1) {
                        return 0;
                }

                /* no memory */
                if (free_frame_ptr == NULL) {
                        return 0;
                }

                spinlock_acquire(&stealmem_lock);

                addr = (free_frame_ptr - frame_table) * PAGE_SIZE; 
                free_frame_ptr = free_frame_ptr->next_free_frame;

                spinlock_release(&stealmem_lock);
        }

        return PADDR_TO_KVADDR(addr);
}

void free_kpages(vaddr_t addr)
{
        paddr_t paddr;
        struct frame_table_entry *to_free;

        if (frame_table == NULL) {
                return;
        }

        paddr = KVADDR_TO_PADDR(addr);

        spinlock_acquire(&stealmem_lock);

        to_free = &frame_table[paddr / PAGE_SIZE];
        to_free->next_free_frame = free_frame_ptr;
        free_frame_ptr = to_free;

        spinlock_release(&stealmem_lock);
}

