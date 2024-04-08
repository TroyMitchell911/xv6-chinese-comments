#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void main();
void timerinit();

// entry.S needs one stack per CPU.
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

// a scratch area per CPU for machine-mode timer interrupts.
uint64 timer_scratch[NCPU][5];

// assembly code in kernelvec.S for machine-mode timer interrupt.
extern void timervec();

// entry.S jumps here in machine mode on stack0.
void
start()
{
  // set M Previous Privilege mode to Supervisor, for mret.
  //获取mstatus寄存器的状态
  unsigned long x = r_mstatus();
  // mstatus中11 12位是上一次跳转过来的级别是什么，我们设置为supervisor态
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  // set M Exception Program Counter to main, for mret.
  // requires gcc -mcmodel=medany
  //将main的地址加载到epc epc寄存器是指如果发生特权级别更改
  // 那么发生特权级别更改前的地址是多少
  w_mepc((uint64)main);

  // disable paging for now.
  //关闭页表功能
  w_satp(0);

  // delegate all interrupts and exceptions to supervisor mode.
  // 将所有中断和异常交给supervisor模式处理
  w_medeleg(0xffff);
  w_mideleg(0xffff);
  // 使能supervisor下的异常、中断、定时器中断
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  // configure Physical Memory Protection to give supervisor mode
  // access to all of physical memory.
  // 允许访问所有128MB物理内存
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  // ask for clock interrupts.
  // 初始化每个hart的定时器
  timerinit();

  // keep each CPU's hartid in its tp register, for cpuid().
  // 存放hart的id到tp寄存器 可以通过cpuid函数读取到
  int id = r_mhartid();
  w_tp(id);

  // switch to supervisor mode and jump to main().
  // 之前设置了ecp也设置了mstatus的MPP位
  // 一个设置了入口函数，一个设置 了我们从supervisor
  // 模式进入的这个machine模式，其实就是虚构一下
  // 假装我们从supervisor模式进来的 这样通过mret
  // 就可进入到main并且进入supervisor模式了
  asm volatile("mret");
}

// arrange to receive timer interrupts.
// they will arrive in machine mode at
// at timervec in kernelvec.S,
// which turns them into software interrupts for
// devintr() in trap.c.
void
timerinit()
{
  // each CPU has a separate source of timer interrupts.
  // 每一个hart都对应一个定时器
  int id = r_mhartid();

  // ask the CLINT for a timer interrupt.
  // 计算定时器间隔，大概100ms一次在qemu的virt虚拟机器中
  int interval = 1000000; // cycles; about 1/10th second in qemu.
  *(uint64*)CLINT_MTIMECMP(id) = *(uint64*)CLINT_MTIME + interval;

  // prepare information in scratch[] for timervec.
  // scratch[0..2] : space for timervec to save registers.
  // scratch[3] : address of CLINT MTIMECMP register.
  // scratch[4] : desired interval (in cycles) between timer interrupts.
  // 0,1,2成员用来中断函数暂存一些数据
  uint64 *scratch = &timer_scratch[id][0];
  // 3成员用来存放定时器到期时间寄存器地址
  scratch[3] = CLINT_MTIMECMP(id);
  // 4成员用来存放定时器出发间隔
  scratch[4] = interval;
  // 将scratch数组的地址放入mscratch，以便定时器中断函数使用就那个
  w_mscratch((uint64)scratch);

  // set the machine-mode trap handler.
  // 写入定时器中断函数的入口地址
  w_mtvec((uint64)timervec);

  // enable machine-mode interrupts.
  // 开启机器模式下的中断全开标志位
  // 类似x86的if标志位
  w_mstatus(r_mstatus() | MSTATUS_MIE);

  // enable machine-mode timer interrupts.
  // 开启定时器中断
  w_mie(r_mie() | MIE_MTIE);
}
