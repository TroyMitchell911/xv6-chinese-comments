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

void
kinit()
{
  // 初始化锁
  initlock(&kmem.lock, "kmem");
  // 清除内存区域
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  // 将当前地址以页为单位向上对齐
  p = (char*)PGROUNDUP((uint64)pa_start);
  // 以页为单位释放内存
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  // 释放的地址要是页的倍数
  // 一定要大于起始地址 小于内存最大地址
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  // 用垃圾数据填充，防止悬挂引用
  memset(pa, 1, PGSIZE);
  // 将内存转换为单向链表结构体
  r = (struct run*)pa;

  acquire(&kmem.lock);
  // 很明显，内存的前8个字节用来存放链表
  // 头插法插入空闲链表
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
  	// 将地址用垃圾数据填充
  	// 很明显，之前空闲链表占用的空间
  	// 也会被擦掉，很巧妙的方法
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
