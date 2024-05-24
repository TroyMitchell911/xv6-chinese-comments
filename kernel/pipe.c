#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"

#define PIPESIZE 512

struct pipe {
  struct spinlock lock;
  char data[PIPESIZE];
  uint nread;     // number of bytes read
  uint nwrite;    // number of bytes written
  int readopen;   // read fd is still open
  int writeopen;  // write fd is still open
};

int
pipealloc(struct file **f0, struct file **f1)
{
  struct pipe *pi;

  pi = 0;
  *f0 = *f1 = 0;
  // 申请两个文件描述符
  if((*f0 = filealloc()) == 0 || (*f1 = filealloc()) == 0)
    goto bad;
  // 申请pipe结构体
  if((pi = (struct pipe*)kalloc()) == 0)
    goto bad;
  pi->readopen = 1;
  pi->writeopen = 1;
  pi->nwrite = 0;
  pi->nread = 0;
  initlock(&pi->lock, "pipe");
  // 0号fd只可读
  (*f0)->type = FD_PIPE;
  (*f0)->readable = 1;
  (*f0)->writable = 0;
  (*f0)->pipe = pi;
  // 1号fd只可写
  (*f1)->type = FD_PIPE;
  (*f1)->readable = 0;
  (*f1)->writable = 1;
  (*f1)->pipe = pi;
  return 0;

 bad:
  if(pi)
    kfree((char*)pi);
  if(*f0)
    fileclose(*f0);
  if(*f1)
    fileclose(*f1);
  return -1;
}

void
pipeclose(struct pipe *pi, int writable)
{
  acquire(&pi->lock);
  if(writable){
    pi->writeopen = 0;
    wakeup(&pi->nread);
  } else {
    pi->readopen = 0;
    wakeup(&pi->nwrite);
  }
  if(pi->readopen == 0 && pi->writeopen == 0){
    release(&pi->lock);
    kfree((char*)pi);
  } else
    release(&pi->lock);
}

int
pipewrite(struct pipe *pi, uint64 addr, int n)
{
  int i = 0;
  struct proc *pr = myproc();
// 获取锁
  acquire(&pi->lock);
  while(i < n){
  	// 判断是否没有只读属性
    if(pi->readopen == 0 || killed(pr)){
      release(&pi->lock);
      return -1;
    }
	// 判断pipe是否已经满了
    if(pi->nwrite == pi->nread + PIPESIZE){ //DOC: pipewrite-full
    // 唤醒因读取pipe进入休眠的进程
      wakeup(&pi->nread);
	// 将当前写入休眠
      sleep(&pi->nwrite, &pi->lock);
    } else {
      char ch;
	  // 拷贝到ch里
      if(copyin(pr->pagetable, &ch, addr + i, 1) == -1)
        break;
	  // 存储数据
      pi->data[pi->nwrite++ % PIPESIZE] = ch;
      i++;
    }
  }
  // 唤醒读线程
  wakeup(&pi->nread);
  release(&pi->lock);

  return i;
}

int
piperead(struct pipe *pi, uint64 addr, int n)
{
  int i;
  struct proc *pr = myproc();
  char ch;
// 获取锁
  acquire(&pi->lock);
// pipe是否只可读并且不为空
  while(pi->nread == pi->nwrite && pi->writeopen){  //DOC: pipe-empty
    if(killed(pr)){
      release(&pi->lock);
      return -1;
    }
	// 睡眠
    sleep(&pi->nread, &pi->lock); //DOC: piperead-sleep
  }
  for(i = 0; i < n; i++){  //DOC: piperead-copy
  // 判断是否为空
    if(pi->nread == pi->nwrite)
      break;
	// 读取
    ch = pi->data[pi->nread++ % PIPESIZE];
	// 复制到用户空间
    if(copyout(pr->pagetable, addr + i, &ch, 1) == -1)
      break;
  }
  // 唤醒写进程
  wakeup(&pi->nwrite);  //DOC: piperead-wakeup
  release(&pi->lock);
  return i;
}
