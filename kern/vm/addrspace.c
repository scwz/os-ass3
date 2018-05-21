/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *        The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>

#define STACK_PAGES 16

/* region flags */
#define REG_R 0x4
#define REG_W 0x2
#define REG_X 0x1

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 *
 * UNSW: If you use ASST3 config as required, then this file forms
 * part of the VM subsystem.
 *
 */

struct addrspace *
as_create(void)
{
        struct addrspace *as;

        as = kmalloc(sizeof(struct addrspace));
        if (as == NULL) {
                return NULL;
        }

        /*
         * Initialize as needed.
         */
        as->regions = NULL;

        return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
        int result;
        struct addrspace *new;
        struct region *curr;

        new = as_create();
        if (new == NULL) {
                return ENOMEM;
        }

        /* add regions from old to new */
        for (curr = old->regions; curr != NULL; curr = curr->next) {
                result = as_define_region(new, 
                                        curr->vbase, 
                                        curr->size, 
                                        curr->accmode & REG_R, 
                                        curr->accmode & REG_W,
                                        curr->accmode & REG_X);
                if (result) {
                        as_destroy(new);
                        return result;
                }
        }

        /* copy old page table entries to new ones */
        result = page_table_copy(old, new);
        if (result) {
                as_destroy(new);
                return result;
        }

        *ret = new;
        return 0;
}

void
as_destroy(struct addrspace *as)
{
        /*
         * Clean up as needed.
         */

        struct region *curr, *next; 

        curr = as->regions;
        while (curr != NULL) {
                next = curr->next;
                kfree(curr);
                curr = next;
        }

        page_table_remove(as);

        kfree(as);
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
        /*
         * Write this. For many designs it won't need to actually do
         * anything. See proc.c for an explanation of why it (might)
         * be needed.
         */
        as_activate();
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
                 int readable, int writeable, int executable)
{
        struct region *curr;

	/* align the region */
	vaddr &= PAGE_FRAME;
        memsize = ROUNDUP(memsize, PAGE_SIZE);

        curr = kmalloc(sizeof(struct region));
        if (curr == NULL) {
                return ENOMEM;
        }

        curr->vbase = vaddr;
        curr->size = memsize;
        curr->accmode = readable | writeable | executable;

        curr->next = as->regions;
        as->regions = curr;

        return 0; 
}

int
as_prepare_load(struct addrspace *as)
{
        /*
         * Write this.
         */
        (void) as;
        return 0;
}

int
as_complete_load(struct addrspace *as)
{
        /*
         * Write this.
         */

        (void)as;
        return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
        int result;

        /* Initial user-level stack pointer */
        *stackptr = USERSTACK;
        result = as_define_region(as, 
                                USERSTACK - STACK_PAGES * PAGE_SIZE, 
                                STACK_PAGES * PAGE_SIZE, 1, 1, 0);

        return result;
}

