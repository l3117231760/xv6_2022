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

struct
{
  struct spinlock lock;
  int count[PHYSTOP/PGSIZE];
}qupage;


void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&qupage.lock,"qupage");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
  {
    acquire(&qupage.lock);
    qupage.count[COW_INDEX((uint64)p)] = 0;
    release(&qupage.lock);
    kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  // printf("kfree\n");
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  r = (struct run*)pa;
  acquire(&qupage.lock);
  if(qupage.count[COW_INDEX((uint64)r)] < 0) panic("kfree qupage.count < 0\n");
  if(qupage.count[COW_INDEX((uint64)r)] != 0)
  {
    qupage.count[COW_INDEX((uint64)r)]--;
    release(&qupage.lock);
    return ;
  }
  release(&qupage.lock);

  acquire(&qupage.lock);
  if(qupage.count[COW_INDEX((uint64)r)] != 0) panic("kfree qupage.count != 0\n");
  release(&qupage.lock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
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
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
  {
    acquire(&qupage.lock);
    qupage.count[COW_INDEX(PGROUNDDOWN((uint64)r))] = 0;
    release(&qupage.lock);
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

void* cowalloc(pagetable_t pagetable,uint64 va)
{
  if(va % PGSIZE != 0)
    return 0;

  // uint64 pa = PTE2PA(*pte);
  uint64 pa = walkaddr(pagetable,va);
  if(pa == 0) 
  {
    // panic("cowalloc: cowalloc pa\n");
    return 0;
  }
  pte_t* pte = walk(pagetable,va,0);
  if(pte == 0) panic("cowalloc pte = 0\n");
  if(!(*pte&PTE_C)) panic("cowalloc: coalloc page have not PTE_C\n");

  acquire(&qupage.lock);
  if(qupage.count[COW_INDEX(pa)] == 0)
  {
    *pte = (*pte & ~(PTE_C)); //去掉 c
    *pte = (*pte | PTE_W);    // 加上 w
    release(&qupage.lock);
    return (void* )pa; 
  }

  if(qupage.count[COW_INDEX(pa)] > 0)
  {
    release(&qupage.lock);
    char *newpage = kalloc();
    if(newpage == 0) return 0;
    memmove(newpage,(char *)pa,PGSIZE);
    *pte = (*pte & ~(PTE_V));  // 去掉 V
    // *pte = (*pte & ~(PTE_C));  // 去掉 C
    // *pte = (*pte |  (PTE_W));  // 加上 W
    uint64 flag = PTE_FLAGS(*pte);
    // mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
    if(mappages(pagetable,va,PGSIZE,(uint64)newpage,(flag | PTE_W) & (~(PTE_C)) )!=0)
    {
      kfree(newpage);
      *pte |= PTE_V;
      // panic("cowalloc mappages : error\n");
      return 0;
    }
    acquire(&qupage.lock);
    qupage.count[COW_INDEX(pa)]--;
    release(&qupage.lock);

    return newpage;
  }
  release(&qupage.lock);
  panic("qupage.count[COW_INDEX(pa)] is miss\n");
  return 0;
}

int iscowpage(pagetable_t pagetable,uint64 va)
{
  if(va >= MAXVA)
    return -1;
  pte_t* pte = walk(pagetable, va, 0);
  if(pte == 0)
  {
    // panic("pet == 0\n");
    return -1;
  }
  if((*pte & PTE_V) == 0)
    return -1;
  return (*pte & PTE_C ? 0 : -1);
}

int pctadd(uint64 pa)
{
  acquire(&qupage.lock);  
  qupage.count[COW_INDEX(PGROUNDDOWN(pa))]++;
  release(&qupage.lock);  
  return 0;
}
