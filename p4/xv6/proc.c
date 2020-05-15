#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "pstat.h"


typedef struct{
    struct proc * queue[NQUEUE];// array
    int front,rear, num;// front and rear        
}priQueue;

//initial queue
void initQueue(priQueue * pQ){
     pQ->front = pQ->rear = pQ->num= 0;
}
 
 //whether queue is empty
int queueEmpty(priQueue * pQ){
    if(pQ->front == pQ->rear){
         return 1;
    }else{
        return 0;      
    }
}
 //push a p to the end of queue
int enterQueue(priQueue *pQ, struct proc * p){
    // judge whether the queue is full
    if((pQ->rear + 1) % (NQUEUE ) == pQ->front){
         return 0;
    }
    // not full, then insert p to the tail
    pQ -> queue[pQ -> rear] = p;
    pQ -> rear = (pQ -> rear + 1) % (NQUEUE);
    pQ -> num++;
    return 1;
}

//delete from priQueue pQ according to process pointer p
int deleteByP(priQueue * pQ, struct proc * p){
  //whether deleted successfully 
  int deleted = 0;
  //if queue is empty
  if(pQ->front == pQ->rear){
    return deleted;
  } else{//not empty
      int end = pQ->rear;
      if (pQ->rear < pQ->front){
        end += NQUEUE;
      }
      for (int i = pQ->front; i < end;i++){
        //find the p
        if (pQ->queue[i % NQUEUE] == p){
          //move all processes in the queue forward one position
          for (int j = i; j < end - 1;j++){
            pQ->queue[j % NQUEUE] = pQ->queue[(j + 1)% NQUEUE];
          }
          //reduce rear
          pQ->num--;
          pQ->rear = (pQ->rear - 1 + NQUEUE)% NQUEUE;
          deleted = 1;
          break;
        }
      }
  }
  return deleted;
}

//get the first p from queue
struct proc * getFront(priQueue * pQ){
  if(pQ->front == pQ->rear){
    return 0;
  }
  return pQ->queue[pQ->front];
}

//delete front q from queue
int deleteQueue(priQueue *pQ, struct proc * p){
    // judge whether the queue is empty
    if(pQ->front == pQ->rear){
         return 0;
    }
    // not empty, delete it
    p = pQ->queue[pQ->front];
    pQ -> front = (pQ -> front + 1) % (NQUEUE);
    pQ -> num--;
    return 1; 
}

//remove all processes that are not runnable
void checkQueue(priQueue *pQ){
  int end = pQ->rear;
  if (pQ->rear < pQ->front){
    end = pQ->rear + NQUEUE;
  }
  for (int i = pQ->front;i < end;i++){
    //find the p that is not runnable
    if ((pQ->queue[i % NQUEUE]->state == ZOMBIE) || 
    (pQ->queue[i % NQUEUE]->state == UNUSED)){
      //move all processes in the queue forward one position
      // cprintf("deleted pid = %d, state = %d\n", 
      // pQ->queue[i % NQUEUE]->pid,
      // pQ->queue[i % NQUEUE]->state);
      for (int j = i; j < end - 1;j++){
        pQ->queue[j % NQUEUE] = pQ->queue[(j + 1)% NQUEUE];
      }
      //reduce rear
      pQ->num--;
      pQ->rear = (pQ->rear - 1 + NQUEUE)% NQUEUE;
      i--;
      end = pQ->rear;
      if (pQ->rear < pQ->front){
        end = pQ->rear + NQUEUE;
      }
    }
  }
}

//just for test
void pprint(priQueue *pQ){
  cprintf("-----------------------\n");
  cprintf("front = %d, rear = %d, num = %d\n", pQ->front, pQ->rear,pQ->num);
  int end = pQ->rear;
  if (pQ->rear < pQ->front){
    end += NQUEUE;
  }
  for (int i = pQ->front; i < end;i++){
    //find the p
    cprintf("pid = %d, state = %d\n", 
    pQ->queue[i % NQUEUE]->pid,
    pQ->queue[i % NQUEUE]->state);
  }
  cprintf("-----------------------\n");
}

//select the first RUNNABLE proc or return 0
struct proc * select(priQueue *pQ){
  struct proc * p = 0;
  int end = pQ->rear;
  if (pQ->rear < pQ->front){
    end += NQUEUE;
  }
  for (int i = pQ->front; i < end;i++){
    //find the p
    if (pQ->queue[i % NQUEUE]->state == RUNNABLE){
      p = pQ->queue[i % NQUEUE];
      return p;
    }
  }
  return 0;
}

// time slice for each priority queue
const int timeslice[NPRI] = {20, 16, 12, 8};

struct {
  struct spinlock lock;
  struct proc proc[NQUEUE];
  priQueue pq[NPRI];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

// Set up first user process.
void
userinit(void)
{
  // initial priority queues
  for (int i = 0; i < NPRI; i++){
    initQueue(&(ptable.pq[i]));
  }

  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;
  
  //set priority-relates info for process
  p->priority = 3;
  for (int i = 0; i < NPRI; i++){
    p->remain[i] = timeslice[i];
    p->qtail[i] = 0;
  }
  if (enterQueue(&(ptable.pq[3]), p) == 0){
    cprintf("fork add pid = %d fail\n",p->pid);
  }
  p->qtail[3]++;
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;
  //set priority-relates info for process
  np->priority = curproc->priority;
  for (int i = 0; i < NPRI; i++){
    np->remain[i] = timeslice[i];
    np->qtail[i] = 0;
  }
  if (enterQueue(&(ptable.pq[curproc->priority]), np) == 0){
    cprintf("Priority queue %d is full when alloc pid = %d\n",curproc->priority, np->pid);
  }
  np->qtail[np->priority]++;
  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        // if (deleteByP(&ptable.pq[p->priority], p) == 0){
        //   cprintf("exit deleteByP pid = %d fail\n", p->pid);
        // }
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  cprintf("scheduler boot success!\n");

  for(;;){
    // Enable interrupts on this processor.
    sti();

    acquire(&ptable.lock);

    // check all queues, remove zombie and 
    for (int i = NPRI - 1; i >=0;i--){
      if (queueEmpty(&ptable.pq[i]) != 1){
        checkQueue(&ptable.pq[i]);
      }
    }

    //find highest priority and runnable proc
    int i = NPRI - 1;
    for (i = NPRI - 1; i >=0;i--){
      p = select(&ptable.pq[i]);
      if (p != 0){
        break;
      }    
    }

    //if the selected is not runnable, just continue
    if ( p == 0 || p -> state != RUNNABLE){
      cli();
      release(&ptable.lock);
      continue;
    }

    //cprintf("pid = %d\n", p->pid);
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;

    swtch(&(c->scheduler), p->context);
    switchkvm();


    //adjust the remaining time and the position of p
    p->remain[i]--;
    p->ticks[i]++;
    if (p->remain[i] == 0){
      p->remain[i] = timeslice[i];
      if (deleteByP(&ptable.pq[i], p) == 0){
        cprintf("scheduler delete pid = %d fail\n",p->pid);
      }
      if (enterQueue(&ptable.pq[i], p) == 0){
        cprintf("scheduler add pid = %d fail\n",p->pid);
      }
      p->qtail[p->priority] ++;
      // if (i == 2){
      //   cprintf("scheduler pid = %d, qtail = %d\n",p->pid, p->qtail[p->priority]);
      // }
    }
    // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;

    release(&ptable.lock);
  }

  
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan){
      p->state = RUNNABLE;
      //put this proc to the back of queue
      if (deleteByP(&ptable.pq[p->priority], p) == 0){
        cprintf("wake deleteByP pid = %d fail\n", p->pid);
      }
      if (enterQueue(&ptable.pq[p->priority], p) == 0){
        cprintf("wake add pid = %d fail\n", p->pid);
      };
      p->remain[p->priority] = timeslice[p->priority];
      p->qtail[p->priority] ++;
      // if (p->priority == 2){
      //   cprintf("wake pid = %d, qtail = %d\n",p->pid, p->qtail[p->priority]);
      // }
    }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("pid: %d state: %s name: %s priority: %d", p->pid, state, p->name, p->priority);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

int
setpri(int pid, int pri)
{
  if (pid < 0 || pri < 0 || pri > 3){
    return -1;
  }

  struct proc *p;

  //cprintf("pri = %d\n", pri);
  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p -> pid == pid) {
      // cprintf("pid = %d, current pri = %d, This is queue\n", 
      // p -> pid, p -> priority);
      // pprint(&ptable.pq[p->priority]);
      deleteByP(&ptable.pq[p->priority], p);
      enterQueue(&ptable.pq[pri], p);
      p->priority = pri;
      // cprintf("pid = %d, current pri = %d, This is queue\n", 
      // p -> pid, p -> priority);
      // pprint(&ptable.pq[p->priority]);
      p->qtail[pri]++;
      // if (p->priority == 2){
      //   cprintf("sepri pid = %d, qtail = %d\n",p->pid, p->qtail[p->priority]);
      // }
      p->remain[pri] = timeslice[pri];
    }
  }
  release(&ptable.lock);
  return 0;
}

// Returns the scheduler priority for a given process id. 
// If pid is invalid or not found, returns -1.
int
getpri(int pid)
{
    struct proc *p;

    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p -> pid == pid) {
            // Correct proc found, can return priority.
            release(&ptable.lock);
            return p -> priority;
        }
    }
    release(&ptable.lock);
    return -1;
}

// Forks process, and sets priority level to parameter passed in.
int
fork2(int priority)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  if (priority < 0 || priority > 3) {
    return -1;
  }

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;
  //set priority-relates info for process
  np->priority = priority;
  for (int i = 0; i < NPRI; i++){
    np->remain[i] = timeslice[i];
    np->qtail[i] = 0;
  }
  if (enterQueue(&(ptable.pq[priority]), np) == 0){
    cprintf("Priority queue is full when alloc pid = %d\n",np->pid);
  }
  np->qtail[np->priority]++;
  // if (np->priority == 2){
  //   cprintf("fork2 pid = %d, qtail = %d\n",np->pid, np->qtail[np->priority]);
  // }
  release(&ptable.lock);

  return pid;
}

// Gets info from the ptable about all running processes and stores it in pstat.
int
getpinfo(struct pstat* pinfo)
{
  //cprintf("pinfo = %d\n", pinfo);
  if (pinfo == 0){
    return -1;
  }
  struct proc *p;
  int curr = 0;

  //cprintf("begin loop\n");
  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; ++p) {
    //cprintf("p = %d, curr = %d\n", p, curr);
    pinfo -> pid[curr] = p->pid;
    if (p -> state == EMBRYO || p -> state == UNUSED || p -> state == ZOMBIE) {
      pinfo -> inuse[curr] = 0;
    } else {
      pinfo -> inuse[curr] = 1;
    }
    pinfo -> priority[curr] = p -> priority;
    pinfo -> state[curr] = p -> state;
    for (int i = 0; i < NPRI; i ++){
      pinfo -> ticks[curr][i] = p->ticks[i];
      pinfo -> qtail[curr][i] = p->qtail[i];
    }
    curr++;
    //cprintf("pid = %d, pri = %d\n", pinfo -> pid[curr], pinfo -> priority[curr]);

  }  
  //pprint(&ptable.pq[3]);
  //cprintf("end loop\n");
  release(&ptable.lock);
  //cprintf("go return!\n");
  return 0;
}

