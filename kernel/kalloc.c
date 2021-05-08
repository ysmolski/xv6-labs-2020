// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
    int refs[((PHYSTOP - KERNBASE)) / PGSIZE + 1];
    struct spinlock lock;
} kpages;


#define PAGENUM(p) (((p) - KERNBASE) / PGSIZE)

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&kpages.lock, "kpages");

  acquire(&kpages.lock);
  for (uint i=0; i < NELEM(kpages.refs); i++)
      kpages.refs[i]=-100; // means uninitialized page
  release(&kpages.lock);

  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  int refs = kdecref((uint64)pa);
  /* if (refs == 0) */
  /*     printf("kfree %p pn %d %d\n", (uint64)pa, PAGENUM((uint64)pa), refs); */ 
  if (refs == -101){
      // call from init
      acquire(&kpages.lock);
      kpages.refs[PAGENUM((uint64)pa)] = 0;
      release(&kpages.lock);
  } else if (refs < 0) {
      panic("attempt to free already unreferenced page");
  } else if (refs > 0) {
      return;
  }

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

int
kincref(uint64 pa){
    int refs;
    acquire(&kpages.lock);
    kpages.refs[PAGENUM(pa)]++;
    refs = kpages.refs[PAGENUM(pa)];
    release(&kpages.lock);
    return refs;
}

int
kdecref(uint64 pa){
    int refs;
    acquire(&kpages.lock);
    kpages.refs[PAGENUM(pa)]--;
    refs = kpages.refs[PAGENUM(pa)];
    release(&kpages.lock);
    return refs;
}


// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
  }
  release(&kmem.lock);

  if(r) {
      int refs = kincref((uint64)r);
      /* printf("kalloc %p pn %d %d\n", (uint64)r, PAGENUM((uint64)r), refs); */ 
      if (refs>1) {
          /* printf("refs %d\n", refs); */
          panic("kalloc: page has too many refs");
      }
      memset((char*)r, 5, PGSIZE); // fill with junk
  }
  return (void*)r;
}
