// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"


void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
  int pid[PGNUM + 1];
} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.pid[PGNUM] = -1;
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{

  struct run *r;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = (struct run*)v;
  //if (V2P(r)>>12 == 0xDFBD){
    //cprintf("kfree frames = %x, pid = %d\n",V2P(r)>>12, kmem.pid[V2P(r)>>12]);
  //}
  kmem.pid[V2P(r)>>12] = -1;
  r->next = kmem.freelist;
  kmem.freelist = r;
  if(kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(int pid)
{
  struct run *r = 0;
  struct run *pre = 0;
  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  while(r){
    //if pid == -2, it can be put anywhere
    if(pid == -2 
    ||//the ahead and behind page should be -1, -2, or pid
      ((kmem.pid[(V2P(r)>>12)+1] == -1 || kmem.pid[(V2P(r)>>12)+1] == -2 
                                || kmem.pid[(V2P(r)>>12)+1] == pid)
        && (kmem.pid[(V2P(r)>>12)-1] == -1 || kmem.pid[(V2P(r)>>12)-1] == -2
                                || kmem.pid[(V2P(r)>>12)-1] == pid))){
      //cprintf("kalloc frames = %x, pid = %d\n",V2P(r)>>12, pid);
      //update pid list
      kmem.pid[V2P(r)>>12] = pid;
      //update freelist
      if (pre){
        pre->next = r->next;
      } else {
        kmem.freelist = r->next;
      }
      break;
    }else{
      //find next page
      pre = r;
      r = r->next;
    }
  }
    
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}

// Finds which process owns each frame of physical memory.
// PARAMS: 
//  frames: array of ints to be filled with list of allocated frame numbers.
//  pids: array of ints to be filled with pid that owns corresponding frame number in frames.
//  numframes: number of elements in frames and pids array.
int 
dump_physmem(int *frames, int *pids, int numframes)
{
  //cprintf("numframes = %d\n", numframes);
  int pp_index = PGNUM;
  int i = 0;
  if(kmem.use_lock)
    acquire(&kmem.lock);

  while(i < numframes && pp_index >= 0){
    if (kmem.pid[pp_index] != -1){
      frames[i] = pp_index;
      pids[i] = kmem.pid[pp_index];
      i++;
    }
    pp_index --;
  }
  if(kmem.use_lock)
    release(&kmem.lock);
  //cprintf("kalloc.c: dump_physmem\n");
  return 0;
}

