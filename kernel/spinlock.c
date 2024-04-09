// Mutual exclusion spin locks.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"

//todo: 为什么解除锁之后才可以开中断

void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
void
acquire(struct spinlock *lk)
{
  // 关闭中断避免死锁
  push_off(); // disable interrupts to avoid deadlock.
  // 检测拥有该锁的cpu是否想二次获取，如果要二次获取的话
  // 就必然死锁，所以直接pannic
  if(holding(lk))
    panic("acquire");

  // On RISC-V, sync_lock_test_and_set turns into an atomic swap:
  //   a5 = 1
  //   s1 = &lk->locked
  //   amoswap.w.aq a5, a5, (s1)
  // 该函数为gcc内置函数，用于将 lk->locked 的值设置为 1，并返回设置之前的值
  // 如果之前的操作返回的值不是 0，即锁已经被其他线程持有
  while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
    ;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen strictly after the lock is acquired.
  // On RISC-V, this emits a fence instruction.
  // 设置内存屏障，告诉编译器，在该行代码之后的操作
  // 不要提前执行，否则编译器可能会进行代码重排序
  // 避免产生并发问题
  __sync_synchronize();

  // Record info about lock acquisition for holding() and debugging.
  // 设置哪个CPU获取了该锁
  lk->cpu = mycpu();
}

// Release the lock.
void
release(struct spinlock *lk)
{
  // 如果该锁没被锁定或者尝试解锁的cpu不是获取锁的cpu
  // 就直接panic就完了
  if(!holding(lk))
    panic("release");
  // 清空获取锁的cpu
  lk->cpu = 0;

  // Tell the C compiler and the CPU to not move loads or stores
  // past this point, to ensure that all the stores in the critical
  // section are visible to other CPUs before the lock is released,
  // and that loads in the critical section occur strictly before
  // the lock is released.
  // On RISC-V, this emits a fence instruction.
  // 内存屏障， 保证严格顺序，具体解释见acquire
  __sync_synchronize();

  // Release the lock, equivalent to lk->locked = 0.
  // This code doesn't use a C assignment, since the C standard
  // implies that an assignment might be implemented with
  // multiple store instructions.
  // On RISC-V, sync_lock_release turns into an atomic swap:
  //   s1 = &lk->locked
  //   amoswap.w zero, zero, (s1)
  // gcc内置函数，同步锁释放
  // 不用c语言语法是因为这条指令在底层会被翻译成
  // 原子操作
  __sync_lock_release(&lk->locked);

  pop_off();
}

// Check whether this cpu is holding the lock.
// Interrupts must be off.
int
holding(struct spinlock *lk)
{
  int r;
  // 如果该锁被锁定，并且持有该锁的cpu是当前cpu
  r = (lk->locked && lk->cpu == mycpu());
  return r;
}

// push_off/pop_off are like intr_off()/intr_on() except that they are matched:
// it takes two pop_off()s to undo two push_off()s.  Also, if interrupts
// are initially off, then push_off, pop_off leaves them off.

void
push_off(void)
{
// 获取原来中断是否关闭开启的
  int old = intr_get();
// 关闭中断
  intr_off();
// 如果嵌套深度是0，也就是第一次关中断
  if(mycpu()->noff == 0)
  	// 将是否开启中断变量设置为old
    mycpu()->intena = old;
  // 将调用深度+1
  mycpu()->noff += 1;
}

void
pop_off(void)
{
  struct cpu *c = mycpu();
  // push的时候肯定就关中断了，所以pop的时候必定是关的
  if(intr_get())
    panic("pop_off - interruptible");
  // 如果调用深度小于1，也就是没push过
  if(c->noff < 1)
    panic("pop_off");
  // 调用深度-1
  c->noff -= 1;
  // 如果调用深度为0了并且第一次push的时候中断是开的
  // 那么就把中断开启
  if(c->noff == 0 && c->intena)
    intr_on();
}
