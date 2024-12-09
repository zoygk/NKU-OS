# Lab4:进程管理

实验目的：

- 了解内核线程创建/执行的管理过程
- 了解内核线程的切换和基本调度过程

## 练习0：填写已有实验

本实验依赖实验2/3。请把你做的实验2/3的代码填入本实验中代码中有“LAB2”,“LAB3”的注释相应部分。

## 练习1：分配并初始化一个进程控制块（需要编码）

>alloc_proc函数（位于kern/process/proc.c中）负责分配并返回一个新的struct proc_struct结构，用于存储新建立的内核线程的管理信息。ucore需要对这个结构进行最基本的初始化，你需要完成这个初始化过程。
>
>【提示】在alloc_proc函数的实现中，需要初始化的proc_struct结构中的成员变量至少包括：state/pid/runs/kstack/need_resched/parent/mm/context/tf/cr3/flags/name。
>
>请在实验报告中简要说明你的设计实现过程。请回答如下问题：
>
>- 请说明proc_struct中struct context context和struct trapframe *tf成员变量含义和在本实验中的作用是啥？（提示通过看代码和编程调试可以判断出来）

### alloc_proc函数

补充该函数如下：

```c
// alloc_proc - alloc a proc_struct and init all fields of proc_struct
static struct proc_struct *
alloc_proc(void) {
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL) {
        proc->state = PROC_UNINIT;                      //状态为未初始化
        proc->pid = -1;                                 //pid为未赋值
        proc->runs = 0;                                 //运行时间为0
        proc->kstack = 0;                               //除了idleproc其他线程的内核栈都要后续分配
        proc->need_resched = 0;                         //不需要调度切换线程
        proc->parent = NULL;                            //没有父线程
        proc->mm = NULL;                                //未分配内存
        memset(&(proc->context), 0, sizeof(struct context));//将上下文变量全部赋值为0，清空
        proc->tf = NULL;                                //初始化没有中断帧
        proc->cr3 = boot_cr3;                           //内核线程的cr3为boot_cr3，即页目录为内核页目录表
        proc->flags = 0;                                //标志位为0
        memset(proc->name, 0, PROC_NAME_LEN+1);         //将线程名变量全部赋值为0，清空
    }
    return proc;
}
```

该函数负责分配一个 proc_struct 并初始化 proc_struct 的所有字段，各个字段初始值代表的含义已在代码注释中给出。

### 请说明proc_struct中struct context context和struct trapframe *tf成员变量含义和在本实验中的作用是啥

#### struct context context

context结构体定义如下：

```c
struct context {
    uintptr_t ra;  //保存返回地址寄存器的值。
    uintptr_t sp;  //保存堆栈指针寄存器的值。
    uintptr_t s0;
    uintptr_t s1;
    uintptr_t s2;
    uintptr_t s3;
    uintptr_t s4;
    uintptr_t s5;
    uintptr_t s6;
    uintptr_t s7;
    uintptr_t s8;
    uintptr_t s9;
    uintptr_t s10;
    uintptr_t s11;  //保存一组保存调用者不破坏的寄存器
};
```

可以看到context中保存了进程执行的上下文，也就是几个关键的寄存器的值。这些寄存器的值用于在进程切换中还原之前进程的运行状态。  

当一个线程被调度器暂停运行时，当前线程的所有关键寄存器值（包括程序计数器、堆栈指针、保存寄存器等）就保存在 context 结构中（保存上下文），这样，当调度器恢复该线程时，就可以使用 switch_to 函数加载 context 中保存的寄存器值（恢复上下文），线程就可以从中断或暂停的位置继续运行。  

在本实验中的作用：  
本次实验中context的作用是保存forkret函数的返回地址，以及forkret函数的参数struct trapframe。
参见copy_thread函数：

```c
static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf) {
    //将 trapframe 存储在进程的内核栈中，具体位置是内核栈的顶部（即栈底部偏移）。
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE - sizeof(struct trapframe));
    *(proc->tf) = *tf;

    // Set a0 to 0 so a child process knows it's just forked
    proc->tf->gpr.a0 = 0;
    proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp;

    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)(proc->tf);
}
```

在这个函数中将context.ra设置为 forkret 函数的地址，让新创建的进程可以从一个统一的函数入口（forket）进入。也就是当调用switch_to函数后，ra寄存器值就会变为context.ra，接着就会跳转到forkret函数。 到了forkret就意味着这次进程切换已经完成。  
将context.sp 设置为目标进程的内核栈中的 trapframe 地址，确保上下文切换时，目标进程能够继续使用正确的栈进行执行。

#### struct trapframe *tf

tf结构体定义如下：

```c
struct trapframe {
    struct pushregs gpr;      // 保存通用寄存器的值
    uintptr_t status;         // 保存进程的状态寄存器值
    uintptr_t epc;            // 异常程序计数器
    uintptr_t badvaddr;       // 错误的虚拟地址（用于存储访问异常时的地址）
    uintptr_t cause;          // 异常原因寄存器
};

```

tf里保存了进程的中断帧。当进程从用户空间跳进内核空间的时候，进程的执行状态被保存在了中断帧中（注意这里需要保存的执行状态数量不同于上下文切换）。系统调用可能会改变用户寄存器的值，我们可以通过调整中断帧来使得系统调用返回特定的值。  
它用于处理器从用户态或内核态陷入中断时保存处理器状态，以及在异常处理完成后恢复状态。在异常、中断或系统调用发生时，CPU会将信息保存到 trapframe 中。  
在copy_theard函数中同样对tf进行了操作

```c
static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf) {
    //将 trapframe 存储在进程的内核栈中，具体位置是内核栈的顶部（即栈底部偏移）。
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE - sizeof(struct trapframe));
    //将传入的 tf的内容复制到当前进程的 proc->tf 中
    *(proc->tf) = *tf;

    // Set a0 to 0 so a child process knows it's just forked
    proc->tf->gpr.a0 = 0;
    //如果 esp 是 0，表示需要使用默认的栈指针值，指向当前的 trapframe 位置。否则，使用传入的 esp
    proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp;

    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)(proc->tf);
}
```

通过这些操作可以确保在发生中断、系统调用或上下文切换时，进程的执行状态能够正确恢复。

## 练习2：为新创建的内核线程分配资源

>创建一个内核线程需要分配和设置好很多资源。kernel_thread函数通过调用do_fork函数完成具体内核线程的创建工作。do_kernel函数会调用alloc_proc函数来分配并初始化一个进程控制块，但alloc_proc只是找到了一小块内存用以记录进程的必要信息，并没有实际分配这些资源。ucore一般通过do_fork实际创建新的内核线程。do_fork的作用是，创建当前内核线程的一个副本，它们的执行上下文、代码、数据都一样，但是存储位置不同。因此，我们实际需要"fork"的东西就是stack和trapframe。在这个过程中，需要给新内核线程分配资源，并且复制原进程的状态。你需要完成在kern/process/proc.c中的do_fork函数中的处理过程。它的大致执行步骤包括：
>
> - 调用alloc_proc，首先获得一块用户信息块。
> - 为进程分配一个内核栈。
> - 复制原进程的内存管理信息到新进程（但内核线程不必做此事）
> - 复制原进程上下文到新进程
> - 将新进程添加到进程列表
> - 唤醒新进程
> - 返回新进程号
>
>请在实验报告中简要说明你的设计实现过程。请回答如下问题：
>
> - 请说明ucore是否做到给每个新fork的线程一个唯一的id？请说明你的分析和理由。

### do_fork函数

#### 设计过程

1. 调用 alloc_proc 分配并初始化proc_struct  
在创建新的内核线程之前，首先需要为该线程分配一个 proc_struct 类型的进程控制块。alloc_proc 函数负责这项工作。若分配失败，直接跳转到 fork_out 标签，进行资源清理。

2. 分配内核栈  
内核线程需要一个独立的内核栈来保存其运行时的上下文。因此，我们调用 setup_kstack 为新进程分配并初始化一个内核栈。如果栈的分配失败，跳转到 bad_fork_cleanup_proc 进行资源回收。

3. 复制内存管理信息  
调用 copy_mm 函数，复制当前进程的内存管理信息到新进程。需要注意的是对于内核线程（clone_flags 带有 CLONE_VM），可能直接共享内存。而对于用户进程，则需要复制内存。如果失败，清理分配的资源。

4. 复制上下文和 trapframe 到新进程  
调用copy_thread 函数根据传入的栈指针 stack 和 trapframe（用于保存中断处理的状态）来复制父进程的上下文，进而保证新进程的运行状态。

5. 分配唯一 PID  
每个进程都需要一个唯一的进程 ID（PID）来标识它。我们使用 get_pid 来为新创建的内核线程分配一个唯一的 PID。

6. 将新进程添加到进程哈希表和全局进程列表  
为了管理所有的进程，我们需要将新进程加入到系统的进程管理结构中。hash_proc 函数会将新进程加入到进程哈希表中，便于根据 PID 快速检索进程。list_add 将新进程添加到全局进程列表 proc_list 中，使其能够被系统调度器调度。

7. 唤醒新进程并设置其为 RUNNABLE  
通过 wakeup_proc 将新进程的状态设置为 PROC_RUNNABLE，意味着该进程已经准备好执行

8. 返回新进程的 PID  
返回新进程的 PID，表示新进程创建成功。

#### 函数具体实现

```c
    //LAB4:EXERCISE2 YOUR CODE
    //    1. call alloc_proc to allocate a proc_struct
    //    2. call setup_kstack to allocate a kernel stack for child process
    //    3. call copy_mm to dup OR share mm according clone_flag
    //    4. call copy_thread to setup tf & context in proc_struct
    //    5. insert proc_struct into hash_list && proc_list
    //    6. call wakeup_proc to make the new child process RUNNABLE
    //    7. set ret vaule using child proc's pid
    //使用 alloc_proc 分配并初始化 proc_struct
    proc = alloc_proc();
    if (proc == NULL) {
        goto fork_out;
    }

    //为进程分配一个内核栈
    if (setup_kstack(proc) != 0) {
        goto bad_fork_cleanup_proc;
    }

    //复制内存管理信息
    if (copy_mm(clone_flags, proc) != 0) {
        goto bad_fork_cleanup_kstack;
    }

    //复制上下文和 trapframe 到新进程
    copy_thread(proc, stack, tf);

    //分配唯一 PID
    proc->pid = get_pid();

    //将新进程添加到进程哈希表和全局进程列表
    hash_proc(proc);
    list_add(&proc_list, &(proc->list_link));

    //唤醒新进程并设置其为 RUNNABLE
    wakeup_proc(proc);

    //返回新进程的 PID
    ret = proc->pid;
```

#### 请说明ucore是否做到给每个新fork的线程一个唯一的id

do_fork函数通过调用get_pid()函数为新进程分配一个唯一的进程ID,该函数如下（包含解释）：

```c
static int
get_pid(void) {
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    //next_safe 和 last_pid 是两个静态变量，用于追踪下一个可用的 PID。next_safe 表示下一次可以安全分配的 PID，last_pid 用来跟踪最后分配的 PID。
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    //last_pid 每次递增，如果 last_pid 超过了最大 PID MAX_PID，则回绕到 1 重新开始分配，使得 PID 在范围内循环
    if (++ last_pid >= MAX_PID) {
        last_pid = 1;
        goto inside;
    }
    //如果 last_pid 大于等于 next_safe，遍历 proc_list 中的所有进程，寻找一个空闲的 PID。
    if (last_pid >= next_safe) {
    inside:
        next_safe = MAX_PID;
    repeat:
    //遍历进程列表，查找当前进程是否已使用 last_pid。如果 last_pid 已被占用，则递增 last_pid 并重新检查。 如果当前进程的 PID 大于 last_pid，并且 next_safe 比当前进程的 PID 大，则更新 next_safe 为当前进程的 PID，表示下一个可用 PID 为当前 PID 之前的一个。
        le = list;
        while ((le = list_next(le)) != list) {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid) {
                if (++ last_pid >= next_safe) {
                    if (last_pid >= MAX_PID) {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid) {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}
```

可以看到get_pid 函数通过遍历进程列表并检查现有进程的 PID，确保每次分配的 PID 是唯一的。

## 练习3：编写proc_run 函数（需要编码）

> proc_run用于将指定的进程切换到CPU上运行。它的大致执行步骤包括:
>
> - 检查要切换的进程是否与当前正在运行的进程相同，如果相同则不需要切换。
> - 禁用中断。你可以使用/kern/sync/sync.h中定义好的宏local_intr_save(x)和local_intr_restore(x)来实现关、开中断。
> - 切换当前进程为要运行的进程。
> - 切换页表，以便使用新进程的地址空间。/libs/riscv.h中提供了lcr3(unsigned int cr3)函数，可实现修改CR3寄存器值的功能。
> - 实现上下文切换。/kern/process中已经预先编写好了switch.S，其中定义了switch_to()函数。可实现两个进程的context切换。
> - 允许中断。
>
>请回答如下问题：
>
> - 在本实验的执行过程中，创建且运行了几个内核线程？
### 函数的实现过程
``` c
if (proc != current) {//判断要切换的进程与当前正在运行进程是否相同
        // LAB4:EXERCISE3 YOUR CODE
        /*
        * Some Useful MACROs, Functions and DEFINEs, you can use them in below implementation.
        * MACROs or Functions:
        *   local_intr_save():        Disable interrupts
        *   local_intr_restore():     Enable Interrupts
        *   lcr3():                   Modify the value of CR3 register
        *   switch_to():              Context switching between two processes
        */
        bool intr_flag;
        struct proc_struct *prev = current;
        local_intr_save(intr_flag);//禁用时钟中断
        {
            current=proc;//切换当前进程为要运行的进程
            lcr3(proc->cr3);//切换页表
            switch_to(&(prev->context),&(proc->context));//此处的参数是新旧进程上下文
        }
        local_intr_restore(intr_flag);//重新开启中断
    }
```
### 本实验在执行过程中分别创建了两个内核线程

分别是idleproc和initproc，这两个内核线程共用一个页表
## 扩展练习 Challenge

> 说明语句local_intr_save(intr_flag);....local_intr_restore(intr_flag);是如何实现开关中断的？
