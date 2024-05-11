#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "elf.h"

static int loadseg(pde_t *, uint64, struct inode *, uint, uint);

int flags2perm(int flags)
{
    int perm = 0;
    if(flags & 0x1)
      perm = PTE_X;
    if(flags & 0x2)
      perm |= PTE_W;
    return perm;
}

int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint64 argc, sz = 0, sp, ustack[MAXARG], stackbase;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  struct proc *p = myproc();
//开始操作
  begin_op();
//获取这个程序的inode
  if((ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);

  // Check ELF header
  // 检查elf头
  if(readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;

  if(elf.magic != ELF_MAGIC)
    goto bad;
//创建一个包含trampoline和trapframe的页表
// 没有释放trapframe而是沿用之前的trapframe 挺巧妙
  if((pagetable = proc_pagetable(p)) == 0)
    goto bad;

  // Load program into memory.
//加载程序到内存中
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
      //检查是否是程序加载段
    if(ph.type != ELF_PROG_LOAD)
      continue;
//	确保加载 到内存中的数据不会超出程序段的内存大小
    if(ph.memsz < ph.filesz)
      goto bad;
//	确保地址不会溢出
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
//	是否是按页对齐
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    uint64 sz1;
	// 为虚拟地址申请物理内存
    if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, flags2perm(ph.flags))) == 0)
      goto bad;
    sz = sz1;
	// 加载段到内存中
    if(loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;
// 获取当前进程
  p = myproc();
  uint64 oldsz = p->sz;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible as a stack guard.
  // Use the second as the user stack.
  // 给两个1k的空间 用作栈，一个不可访问，作为guard
  sz = PGROUNDUP(sz);
  uint64 sz1;
  if((sz1 = uvmalloc(pagetable, sz, sz + 2*PGSIZE, PTE_W)) == 0)
    goto bad;
  sz = sz1;
  // 清除用户访问标志位
  uvmclear(pagetable, sz-2*PGSIZE);

  sp = sz;
  // 栈底
  stackbase = sp - PGSIZE;

  // Push argument strings, prepare rest of stack in ustack.
  // 压入参数到栈中
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp -= strlen(argv[argc]) + 1;
	// 十六字节对齐
    sp -= sp % 16; // riscv sp must be 16-byte aligned
    // 超出栈底
    if(sp < stackbase)
      goto bad;
    if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
	// ustack数组保存每个参数对应的sp指针
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  // push the array of argv[] pointers.
  // 压入ustack
  sp -= (argc+1) * sizeof(uint64);
  sp -= sp % 16;
  if(sp < stackbase)
    goto bad;
  if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
    goto bad;

  // arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.
  // 写入main函数参数1 参数0由系统调用返回 写入a0
  p->trapframe->a1 = sp;

  // Save program name for debugging.
  // 寻找程序名字
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
	// 写入程序名字
  safestrcpy(p->name, last, sizeof(p->name));

  // Commit to the user image.
  oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->sz = sz;
  p->trapframe->epc = elf.entry;  // initial program counter = main
  p->trapframe->sp = sp; // initial stack pointer
  proc_freepagetable(oldpagetable, oldsz);

  return argc; // this ends up in a0, the first argument to main(argc, argv)

 bad:
  if(pagetable)
    proc_freepagetable(pagetable, sz);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}

// Load a program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
// 将程序段加载到虚拟地址 va 处的页表中。
// va 必须页对齐
// 并且从 va 到 va+sz 的页面必须已经被映射。
// 成功返回0，失败返回-1。

static int
loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint sz)
{
  uint i, n;
  uint64 pa;

  for(i = 0; i < sz; i += PGSIZE){
  	// 寻找va对应的物理地址
    pa = walkaddr(pagetable, va + i);
    if(pa == 0)
      panic("loadseg: address should exist");
	// 如何最后小于一页
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
//	读取内容到虚拟地址中
    if(readi(ip, 0, (uint64)pa, offset+i, n) != n)
      return -1;
  }

  return 0;
}
