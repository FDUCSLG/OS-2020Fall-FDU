#include "string.h"
#include "mmu.h"

#include "console.h"
#include "kalloc.h"

extern char end[];

// Free page's list element struct.
// We store each free page's run structure in the free page itself.
struct run {
	struct run *next;
};

struct {
	struct run *free_list; // Free list of physical pages
} kmem;

// Initialization happens in two phases.
// 1. Call boot_alloc_init() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. Call alloc_init() with the rest of the physical pages after
// installing a full page table.
void
boot_alloc_init(void)
{
	// TODO: Your code here.
}

void
alloc_init(void)
{
	// TODO: Your code here.
}

// Free the page of physical memory pointed at by v.
void
kfree(char *v)
{
	struct run *r;

	if ((uint32_t)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
		panic("kfree");

	// Fill with junk to catch dangling refs.
	memset(v, 1, PGSIZE);

	r = (struct run *)v;
	r->next = kmem.free_list;
	kmem.free_list = r;
}

void
free_range(void *vstart, void *vend)
{
	char *p;
	p = ROUNDUP((char *)vstart, PGSIZE);
	for (; p + PGSIZE <= (char *)vend; p += PGSIZE)
		kfree(p);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char *
kalloc(void)
{
	// TODO: Your code here.
}

// --------------------------------------------------------------
// Checking functions.
// --------------------------------------------------------------

//
// Check that the pages on the kmem.free_list are reasonable.
// TODO: There is only a simple test code here.
// 		 Please add more test code.
//
void
check_free_list(void)
{
	struct run *p;
	if (!kmem.free_list)
		panic("'kmem.free_list' is a null pointer!");

	cprintf("0x%x\n", end);

	for (p = kmem.free_list; p; p = p->next) {
		cprintf("0x%x\n", p);
		assert((void *)p > (void *)end);
	}
}