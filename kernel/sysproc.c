#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  backtrace();

  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint64 buf_addr;
  int count;
  uint64 nbit = 0;
  uint64 bitaddr;
  struct proc * p = myproc();
  argaddr(0,&buf_addr);
  argint(1,&count);
  argaddr(2,&bitaddr);
  for(int i = 0;i < count;i++)
  {
   pte_t *index = walk(p->pagetable,buf_addr + i*PGSIZE,0);
   if((*index & PTE_V) && (*index & PTE_D) )
   {
    // printf("%d\n",i);
    nbit = ( nbit + (1<<(i)) );
   } 
    *index = (*index & ~(*index & PTE_D)); // 一定要清楚
  }
  // if (nbit == ((1 << 1) | (1 << 2) | (1 << 30)))
  // {
  //   printf("OK\n");
  // }else
  // {
  //   printf("false\n");
  // }
  copyout(p->pagetable,bitaddr,(void *)&nbit,sizeof(nbit));
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 
sys_sigalarm(void)
{
  // TODO:
  uint64 my_handler;
  int limit;
  argint(0,&limit);
  argaddr(1,&my_handler);
  // printf("%d limit \n",limit);
  // printf("%d handler \n",my_handler);
  struct proc* p = myproc();
  if(limit != 0 || my_handler!= 0)
  {
    acquire(&p->lock);
    p->interenable = 1;
    p->interval = 0;
    p->timelimit = limit;
    p->hanler = (void *)my_handler;
    p->call = 0; 
    release(&p->lock);
  }
  return 0;
}
uint64 sys_sigreturn(void)
{
  // TODO:
  struct proc *p = myproc();
  p->call = 0;
  acquire(&p->lock);
  memmove(p->trapframe,&p->save,sizeof(struct trapframe));
  release(&p->lock);
  return p->trapframe->a0; // 这里要返回a0的值不能反回0不然会改变a0寄存器改变状态机的状态
}
