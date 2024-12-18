#include <proc.h>
#include <kmalloc.h>
#include <string.h>
#include <sync.h>
#include <pmm.h>
#include <error.h>
#include <sched.h>
#include <elf.h>
#include <vmm.h>
#include <trap.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

/* ------------- process/thread mechanism design&implementation -------------
(an simplified Linux process/thread mechanism )
introduction:
  ucore implements a simple process/thread mechanism. process contains the independent memory sapce, at least one threads
for execution, the kernel data(for management), processor state (for context switch), files(in lab6), etc. ucore needs to
manage all these details efficiently. In ucore, a thread is just a special kind of process(share process's memory).
------------------------------
process state       :     meaning               -- reason
    PROC_UNINIT     :   uninitialized           -- alloc_proc
    PROC_SLEEPING   :   sleeping                -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   runnable(maybe running) -- proc_init, wakeup_proc, 
    PROC_ZOMBIE     :   almost dead             -- do_exit

-----------------------------
process state changing:
                                            
  alloc_proc                                 RUNNING
      +                                   +--<----<--+
      +                                   + proc_run +
      V                                   +-->---->--+ 
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  + 
                                           -----------------------wakeup_proc----------------------------------
-----------------------------
process relations
parent:           proc->parent  (proc is children)
children:         proc->cptr    (proc is parent)
older sibling:    proc->optr    (proc is younger sibling)
younger sibling:  proc->yptr    (proc is older sibling)
-----------------------------
related syscall for process:
SYS_exit        : process exit,                           -->do_exit
SYS_fork        : create child process, dup mm            -->do_fork-->wakeup_proc
SYS_wait        : wait process                            -->do_wait
SYS_exec        : after fork, process execute a program   -->load a program and refresh the mm
SYS_clone       : create child thread                     -->do_fork-->wakeup_proc
SYS_yield       : process flag itself need resecheduling, -- proc->need_sched=1, then scheduler will rescheule this process
SYS_sleep       : process sleep                           -->do_sleep 
SYS_kill        : kill process                            -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit   
SYS_getpid      : get the process's pid
系统调用(syscall)，是用户态(U mode)的程序获取内核态（S mode)服务的方法，所以需要在用户态和内核态都加入对应的支持和处理。我们也可以认为用户态只是提供一个调用的接口，真正的处理都在内核态进行。
我们注意在用户态进行系统调用的核心操作是，通过内联汇编进行ecall环境调用。这将产生一个trap, 进入S mode进行异常处理。

ecall 是 环境调用（Environment Call）的缩写，主要用于 RISC-V 架构。
ecall 是 RISC-V 中的一种指令，用于从 用户模式 切换到 特权模式（通常是内核模式），即执行系统调用。在 RISC-V 架构中，ecall 被用于触发环境调用，从而实现用户空间程序对内核服务的请求。
在 RISC-V 中，ecall 通常在用户空间程序通过调用操作系统服务（如 I/O 操作、内存管理等）时触发。
通过 ecall，用户程序可以请求内核执行诸如进程控制、文件操作、内存管理等服务。
当执行到 ecall 指令时，CPU 会触发一个 异常（trap），并将控制权转移到内核的特定位置，通常是 异常向量。
*/

// the process set's list
list_entry_t proc_list;

#define HASH_SHIFT          10
#define HASH_LIST_SIZE      (1 << HASH_SHIFT)
#define pid_hashfn(x)       (hash32(x, HASH_SHIFT))

// has list for process set based on pid
static list_entry_t hash_list[HASH_LIST_SIZE];

// idle proc
struct proc_struct *idleproc = NULL;
// init proc
struct proc_struct *initproc = NULL;
// current proc
struct proc_struct *current = NULL;

static int nr_process = 0;

void kernel_thread_entry(void);
void forkrets(struct trapframe *tf);
void switch_to(struct context *from, struct context *to);

// alloc_proc - alloc a proc_struct and init all fields of proc_struct
static struct proc_struct *
alloc_proc(void) {
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL) {
    //LAB4:EXERCISE1 YOUR CODE
    /*
     * below fields in proc_struct need to be initialized
     *       enum proc_state state;                      // Process state
     *       int pid;                                    // Process ID
     *       int runs;                                   // the running times of Proces
     *       uintptr_t kstack;                           // Process kernel stack
     *       volatile bool need_resched;                 // bool value: need to be rescheduled to release CPU?
     *       struct proc_struct *parent;                 // the parent process
     *       struct mm_struct *mm;                       // Process's memory management field
     *       struct context context;                     // Switch here to run process
     *       struct trapframe *tf;                       // Trap frame for current interrupt
     *       uintptr_t cr3;                              // CR3 register: the base addr of Page Directroy Table(PDT)
     *       uint32_t flags;                             // Process flag
     *       char name[PROC_NAME_LEN + 1];               // Process name
     */

     //LAB5 YOUR CODE : (update LAB4 steps)
     /*
     * below fields(add in LAB5) in proc_struct need to be initialized  
     *       uint32_t wait_state;                        // waiting state
     *       struct proc_struct *cptr, *yptr, *optr;     // relations between processes
     */
      proc->state = PROC_UNINIT;
      // 获取一个唯一的 PID
      proc->pid = -1; // 在稍后调用 get_pid 时分配实际 PID
      // 初始化运行次数为 0
      proc->runs = 0;
      // 分配内核栈
      proc->kstack = 0; // 使用 setup_kstack 函数时会赋值
      // 初始化是否需要重新调度的标志为 false
      proc->need_resched = 0;
      // 设置父进程为 NULL
      proc->parent = NULL;
      // 初始化内存管理字段为 NULL
      proc->mm = NULL;
      // 初始化上下文
      memset(&(proc->context), 0, sizeof(struct context));
      // 初始化中断帧为 NULL
      proc->tf = NULL;
      // 设置 CR3 寄存器值为 0，稍后分配页目录时会更新
      proc->cr3 = boot_cr3;
      // 初始化标志位为 0
      proc->flags = 0;
      // 初始化进程名称为空字符串
      memset(proc->name, 0, sizeof(proc->name));
      proc->wait_state = 0;
      proc->cptr = NULL;// Child Pointer 表示当前进程的子进程
      proc->optr = NULL;// Older Sibling Pointer 表示当前进程的上一个兄弟进程
      proc->yptr = NULL;// Younger Sibling Pointer 表示当前进程的下一个兄弟进程
    }
    return proc;
}

// set_proc_name - set the name of proc
char *
set_proc_name(struct proc_struct *proc, const char *name) {
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

// get_proc_name - get the name of proc
char *
get_proc_name(struct proc_struct *proc) {
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

// set_links - set the relation links of process
static void
set_links(struct proc_struct *proc) {
    list_add(&proc_list, &(proc->list_link));
    proc->yptr = NULL;
    if ((proc->optr = proc->parent->cptr) != NULL) {
        proc->optr->yptr = proc;
    }
    proc->parent->cptr = proc;
    nr_process ++;
}

// remove_links - clean the relation links of process
static void
remove_links(struct proc_struct *proc) {
    list_del(&(proc->list_link));
    if (proc->optr != NULL) {
        proc->optr->yptr = proc->yptr;
    }
    if (proc->yptr != NULL) {
        proc->yptr->optr = proc->optr;
    }
    else {
       proc->parent->cptr = proc->optr;
    }
    nr_process --;
}

// get_pid - alloc a unique pid for process
static int
get_pid(void) {
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    if (++ last_pid >= MAX_PID) {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe) {
    inside:
        next_safe = MAX_PID;
    repeat:
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

// proc_run - make process "proc" running on cpu
// NOTE: before call switch_to, should load  base addr of "proc"'s new PDT
void
proc_run(struct proc_struct *proc) {
    if (proc != current) {
        // LAB4:EXERCISE3 YOUR CODE
        /*
        * Some Useful MACROs, Functions and DEFINEs, you can use them in below implementation.
        * MACROs or Functions:
        *   local_intr_save():        Disable interrupts
        *   local_intr_restore():     Enable Interrupts
        *   lcr3():                   Modify the value of CR3 register
        *   switch_to():              Context switching between two processes
        */
        unsigned long flags;
        local_intr_save(flags);

        // 2. 切换当前进程为指定进程
        struct proc_struct *prev = current;
        current = proc;

        // 3. 切换页表，修改CR3寄存器
        lcr3(proc->cr3);

        // 4. 执行上下文切换
        switch_to(&(prev->context), &(current->context));

        // 5. 恢复中断状态
        local_intr_restore(flags);
    }
}

// forkret -- the first kernel entry point of a new thread/process
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the current proc will execute here.
static void
forkret(void) {
    forkrets(current->tf);
}

// hash_proc - add proc into proc hash_list
static void
hash_proc(struct proc_struct *proc) {
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// unhash_proc - delete proc from proc hash_list
static void
unhash_proc(struct proc_struct *proc) {
    list_del(&(proc->hash_link));
}

// find_proc - find proc frome proc hash_list according to pid
struct proc_struct *
find_proc(int pid) {
    if (0 < pid && pid < MAX_PID) {
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
        while ((le = list_next(le)) != list) {
            struct proc_struct *proc = le2proc(le, hash_link);
            if (proc->pid == pid) {
                return proc;
            }
        }
    }
    return NULL;
}

// kernel_thread - create a kernel thread using "fn" function
// NOTE: the contents of temp trapframe tf will be copied to 
//       proc->tf in do_fork-->copy_thread function
int
kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags) {
    struct trapframe tf;
    memset(&tf, 0, sizeof(struct trapframe));
    tf.gpr.s0 = (uintptr_t)fn;
    tf.gpr.s1 = (uintptr_t)arg;
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE;
    tf.epc = (uintptr_t)kernel_thread_entry;
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}

// setup_kstack - alloc pages with size KSTACKPAGE as process kernel stack
static int
setup_kstack(struct proc_struct *proc) {
    struct Page *page = alloc_pages(KSTACKPAGE);
    if (page != NULL) {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}

// put_kstack - free the memory space of process kernel stack
static void
put_kstack(struct proc_struct *proc) {
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}

// setup_pgdir - alloc one page as PDT
static int
setup_pgdir(struct mm_struct *mm) {
    struct Page *page;
    if ((page = alloc_page()) == NULL) {
        return -E_NO_MEM;
    }
    pde_t *pgdir = page2kva(page);
    memcpy(pgdir, boot_pgdir, PGSIZE);

    mm->pgdir = pgdir;
    return 0;
}

// put_pgdir - free the memory space of PDT
static void
put_pgdir(struct mm_struct *mm) {
    free_page(kva2page(mm->pgdir));
}

// copy_mm - process "proc" duplicate OR share process "current"'s mm according clone_flags
//         - if clone_flags & CLONE_VM, then "share" ; else "duplicate"
static int
copy_mm(uint32_t clone_flags, struct proc_struct *proc) {
    struct mm_struct *mm, *oldmm = current->mm;

    /* current is a kernel thread */
    if (oldmm == NULL) {
        return 0;
    }
    if (clone_flags & CLONE_VM) {
        mm = oldmm;
        goto good_mm;
    }
    int ret = -E_NO_MEM;
    if ((mm = mm_create()) == NULL) {
        goto bad_mm;
    }
    if (setup_pgdir(mm) != 0) {
        goto bad_pgdir_cleanup_mm;
    }
    lock_mm(oldmm);
    {
        ret = dup_mmap(mm, oldmm);
    }
    unlock_mm(oldmm);

    if (ret != 0) {
        goto bad_dup_cleanup_mmap;
    }

good_mm:
    mm_count_inc(mm);
    proc->mm = mm;
    proc->cr3 = PADDR(mm->pgdir);
    return 0;
bad_dup_cleanup_mmap:
    exit_mmap(mm);
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    return ret;
}

// copy_thread - setup the trapframe on the  process's kernel stack top and
//             - setup the kernel entry point and stack of process
static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf) {
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
    *(proc->tf) = *tf;

    // Set a0 to 0 so a child process knows it's just forked
    proc->tf->gpr.a0 = 0;
    proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp;

    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)(proc->tf);
}

/* do_fork -     父进程创建一个新的子进程
 * @clone_flags: 用于指导如何克隆子进程
 * @stack:       父进程的用户栈指针。如果 stack==0，意味着创建的是一个内核线程。
 * @tf:          要复制到子进程 proc->tf 的 trapframe 信息
 * 从当前进程（父进程）复制一些资源并创建一个新的进程（子进程）。在该过程中，操作系统会分配资源（如内存、进程表项等），并为新进程设置必要的执行环境
 */
int
do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS) {
        goto fork_out;
    }// 如果进程数量超过最大值，则无法创建新进程，返回错误
    ret = -E_NO_MEM;
    //LAB4:EXERCISE2 YOUR CODE
    /*
     * Some Useful MACROs, Functions and DEFINEs, you can use them in below implementation.
     * MACROs or Functions:
     *   alloc_proc:   create a proc struct and init fields (lab4:exercise1)
     *   setup_kstack: alloc pages with size KSTACKPAGE as process kernel stack
     *   copy_mm:      process "proc" duplicate OR share process "current"'s mm according clone_flags
     *                 if clone_flags & CLONE_VM, then "share" ; else "duplicate"
     *   copy_thread:  setup the trapframe on the  process's kernel stack top and
     *                 setup the kernel entry point and stack of process
     *   hash_proc:    add proc into proc hash_list
     *   get_pid:      alloc a unique pid for process
     *   wakeup_proc:  set proc->state = PROC_RUNNABLE
     * VARIABLES:
     *   proc_list:    the process set's list
     *   nr_process:   the number of process set
     */

    //    1. call alloc_proc to allocate a proc_struct
    //    2. call setup_kstack to allocate a kernel stack for child process
    //    3. call copy_mm to dup OR share mm according clone_flag
    //    4. call copy_thread to setup tf & context in proc_struct
    //    5. insert proc_struct into hash_list && proc_list
    //    6. call wakeup_proc to make the new child process RUNNABLE
    //    7. set ret vaule using child proc's pid

    //LAB5 YOUR CODE : (update LAB4 steps)
    //TIPS: you should modify your written code in lab4(step1 and step5), not add more code.
   /* Some Functions
    *    set_links:  set the relation links of process.  ALSO SEE: remove_links:  lean the relation links of process 
    *    -------------------
    *    update step 1: set child proc's parent to current process, make sure current process's wait_state is 0
    *    update step 5: insert proc_struct into hash_list && proc_list, set the relation links of process
    */
    proc = alloc_proc();
    if (proc == NULL) {
        goto fork_out;
    }// 创建进程结构体，分配内存并初始化字段
    // 设置子进程的父进程为当前进程
    proc->parent = current;
    assert(current->wait_state == 0);  // wait_state == 0 表示当前进程处于可执行状态，不在等待任何外部事件或资源。这意味着当前进程不在等待其他进程的操作，也不在等待某些资源的释放。
    //如果 wait_state 的值为其他值，则表示当前进程可能正在等待某个特定的事件发生，如 I/O 操作完成、子进程结束、某个锁的释放等。

    // 为子进程分配内核栈
    if (setup_kstack(proc) != 0) {
        goto bad_fork_cleanup_proc;
    }

    //复制内存管理信息
    //copy_mm->dup_mmap->copy_range
    if (copy_mm(clone_flags, proc) != 0) {
        goto bad_fork_cleanup_kstack;
    }
    // 设置子进程的 trapframe 信息，准备上下文切换
    copy_thread(proc, stack, tf);
    bool intr_flag;
    local_intr_save(intr_flag);  // 保存当前的中断标志
    {
        // 为子进程分配唯一的进程 ID
        proc->pid = get_pid();
        // 设置进程的关系链
        set_links(proc);
        // 将子进程插入进程哈希链表和进程列表
        hash_proc(proc);
    }
    local_intr_restore(intr_flag);  // 恢复中断标志
    wakeup_proc(proc);
    ret = proc->pid;
fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}

/*
do_exit 函数 — 由 sys_exit 调用：

调用 exit_mmap、put_pgdir 和 mm_destroy 来释放进程几乎所有的内存空间。
设置进程状态为 PROC_ZOMBIE，然后调用 wakeup_proc(parent) 来通知父进程回收资源。
调用调度器 scheduler，切换到其他进程。
*/
int do_exit(int error_code) {
    // 检查当前进程是否为 idleproc 或 initproc，如果是则无法退出
    if (current == idleproc) {
        panic("idleproc exit.\n");  // 空闲进程不能退出，直接触发 panic
    }
    if (current == initproc) {
        panic("initproc exit.\n");  // init 进程不能退出，直接触发 panic
    }

    // 获取当前进程的内存管理结构
    struct mm_struct *mm = current->mm;

    // 如果当前进程有内存管理结构，释放内存
    if (mm != NULL) {
        lcr3(boot_cr3);  // 恢复内核页目录
        if (mm_count_dec(mm) == 0) {  // 如果进程内存引用计数为 0
            exit_mmap(mm);  // 释放进程的内存映射
            put_pgdir(mm);  // 释放进程的页目录
            mm_destroy(mm);  // 销毁进程的内存管理结构
        }
        current->mm = NULL;  // 清空当前进程的内存管理结构
    }

    // 设置进程的状态为僵尸进程，并记录退出码
    current->state = PROC_ZOMBIE;
    current->exit_code = error_code;

    bool intr_flag;
    struct proc_struct *proc;
    local_intr_save(intr_flag);  // 保存当前中断状态
    {
        // 如果当前进程有父进程，唤醒父进程
        proc = current->parent;
        if (proc->wait_state == WT_CHILD) {
            wakeup_proc(proc);  // 唤醒父进程，使其处理子进程的退出
        }

        // 遍历当前进程的所有子进程，将它们移交给 init 进程
        while (current->cptr != NULL) {
            proc = current->cptr;
            current->cptr = proc->optr;

            proc->yptr = NULL;
            if ((proc->optr = initproc->cptr) != NULL) {
                initproc->cptr->yptr = proc;  // 将子进程加入到 init 进程的子进程链表
            }
            proc->parent = initproc;  // 子进程的父进程设为 init 进程
            initproc->cptr = proc;  // 将子进程挂载到 init 进程

            // 如果子进程已经是僵尸进程，唤醒 init 进程
            if (proc->state == PROC_ZOMBIE) {
                if (initproc->wait_state == WT_CHILD) {
                    wakeup_proc(initproc);  // 唤醒 init 进程
                }
            }
        }
    }
    local_intr_restore(intr_flag);  // 恢复中断状态

    // 调用调度器选择下一个进程执行
    schedule();

    // 如果程序执行到这里，说明调度器发生了问题，应该触发 panic
    panic("do_exit will not return!! %d.\n", current->pid);
}


/* load_icode - 将二进制程序（ELF 格式）的内容加载为当前进程的新内容
 * @binary:  二进制程序内容的内存地址
 * @size:  二进制程序内容的大小
 */
static int load_icode(unsigned char *binary, size_t size) {
    if (current->mm != NULL) {
        panic("load_icode: current->mm must be empty.\n");
    }

    int ret = -E_NO_MEM;
    struct mm_struct *mm;
    
    // (1) 创建一个新的 mm（内存管理）结构，用于当前进程
    if ((mm = mm_create()) == NULL) {
        goto bad_mm;  // 如果内存不足以分配 mm，进入错误处理
    }
    
    // (2) 创建一个新的页目录（PDT），并将 mm->pgdir 设置为该页目录的内核虚拟地址
    if (setup_pgdir(mm) != 0) {
        goto bad_pgdir_cleanup_mm;  // 如果页目录设置失败，进入错误处理
    }
    
    // (3) 将二进制程序的 TEXT/DATA 部分复制到进程的内存空间，并构建 BSS 部分
    struct Page *page;
    
    // (3.1) 获取 ELF 文件头（ELF 格式）
    struct elfhdr *elf = (struct elfhdr *)binary;
    
    // (3.2) 获取 ELF 程序头（Program Header）的入口
    struct proghdr *ph = (struct proghdr *)(binary + elf->e_phoff);
    
    // (3.3) 检查 ELF 文件的有效性
    if (elf->e_magic != ELF_MAGIC) {
        ret = -E_INVAL_ELF;
        goto bad_elf_cleanup_pgdir;  // 如果 ELF 文件无效，进入错误处理
    }

    uint32_t vm_flags, perm;
    struct proghdr *ph_end = ph + elf->e_phnum;
    
    // 遍历所有的程序段（Program Header）
    for (; ph < ph_end; ph++) {
        // (3.4) 找到所有类型为 ELF_PT_LOAD 的程序段
        if (ph->p_type != ELF_PT_LOAD) {
            continue;
        }
        
        // (3.5) 检查文件大小和内存大小的有效性
        if (ph->p_filesz > ph->p_memsz) {
            ret = -E_INVAL_ELF;
            goto bad_cleanup_mmap;  // 如果文件大小大于内存大小，进入错误处理
        }
        
        // (3.6) 为该段设置虚拟内存区域（VMA）
        vm_flags = 0, perm = PTE_U | PTE_V;
        if (ph->p_flags & ELF_PF_X) vm_flags |= VM_EXEC;
        if (ph->p_flags & ELF_PF_W) vm_flags |= VM_WRITE;
        if (ph->p_flags & ELF_PF_R) vm_flags |= VM_READ;
        
        // 修改权限位，针对 RISC-V 平台的特殊需求
        if (vm_flags & VM_READ) perm |= PTE_R;
        if (vm_flags & VM_WRITE) perm |= (PTE_W | PTE_R);
        if (vm_flags & VM_EXEC) perm |= PTE_X;
        
        // 调用 mm_map 函数为该程序段设置内存映射
        if ((ret = mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL)) != 0) {
            goto bad_cleanup_mmap;
        }

        unsigned char *from = binary + ph->p_offset;
        size_t off, size;
        uintptr_t start = ph->p_va, end, la = ROUNDDOWN(start, PGSIZE);

        ret = -E_NO_MEM;
        
        // (3.6.1) 为程序段分配内存，并将 TEXT/DATA 部分从二进制程序复制到进程内存
        end = ph->p_va + ph->p_filesz;
        while (start < end) {
            if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL) {
                goto bad_cleanup_mmap;
            }
            off = start - la, size = PGSIZE - off, la += PGSIZE;
            if (end < la) {
                size -= la - end;
            }
            memcpy(page2kva(page) + off, from, size);
            start += size, from += size;
        }

        // (3.6.2) 为 BSS 部分分配内存并初始化为零
        end = ph->p_va + ph->p_memsz;
        if (start < la) {
            if (start == end) {
                continue;
            }
            off = start + PGSIZE - la, size = PGSIZE - off;
            if (end < la) {
                size -= la - end;
            }
            memset(page2kva(page) + off, 0, size);
            start += size;
            assert((end < la && start == end) || (end >= la && start == la));
        }
        while (start < end) {
            if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL) {
                goto bad_cleanup_mmap;
            }
            off = start - la, size = PGSIZE - off, la += PGSIZE;
            if (end < la) {
                size -= la - end;
            }
            memset(page2kva(page) + off, 0, size);
            start += size;
        }
    }

    // (4) 为用户栈分配内存
    vm_flags = VM_READ | VM_WRITE | VM_STACK;
    if ((ret = mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, vm_flags, NULL)) != 0) {
        goto bad_cleanup_mmap;
    }
    // 确保分配了足够的栈空间
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 2 * PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 3 * PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 4 * PGSIZE, PTE_USER) != NULL);

    // (5) 设置当前进程的 mm、sr3，并将 CR3 寄存器设置为页目录的物理地址
    mm_count_inc(mm);
    current->mm = mm;
    current->cr3 = PADDR(mm->pgdir);
    lcr3(PADDR(mm->pgdir));

    // (6) 设置进程的 trapframe 以便于用户环境的恢复
    struct trapframe *tf = current->tf;
    uintptr_t sstatus = tf->status;  // 保存 sstatus
    memset(tf, 0, sizeof(struct trapframe));
    /* LAB5:EXERCISE1 你的代码
 * 应该设置 tf->gpr.sp, tf->epc, tf->status
 * 注意：如果我们正确地设置了 trapframe，那么用户级进程就可以从内核返回到用户模式。因此：
 *          tf->gpr.sp 应该是用户栈的顶部（即 sp 的值）
 *          tf->epc 应该是用户程序的入口点（即 sepc 的值）
 *          tf->status 应该是适用于用户程序的状态（即 sstatus 的值）
 *          提示：检查 SSTATUS 中 SPP 和 SPIE 的含义，可以通过 SSTATUS_SPP 和 SSTATUS_SPIE（在 risv.h 中定义）来使用它们
 */
    // 设置 tf 的各项寄存器
    tf->gpr.sp = USTACKTOP;  // 设置用户栈指针
    tf->epc = elf->e_entry;  // 设置程序的入口点
    tf->status = sstatus & ~(SSTATUS_SPP | SSTATUS_SPIE);  // 设置状态寄存器

    ret = 0;
out:
    return ret;

bad_cleanup_mmap:
    exit_mmap(mm);
bad_elf_cleanup_pgdir:
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    goto out;
}


// do_execve - 调用 exit_mmap(mm) 和 put_pgdir(mm) 来回收当前进程的内存空间
//           - 调用 load_icode 来根据二进制程序设置新的内存空间
/*
do_execve 是处理“执行程序”系统调用的内核函数。它完成了将当前进程的内存空间清理并加载一个新的程序到当前进程中。这是一个“进程替换”操作，也就是一个进程会被新的程序替代，但进程本身的 PID 等标识符不变。
*/
int do_execve(const char *name, size_t len, unsigned char *binary, size_t size) {
    // 获取当前进程的内存管理结构体
    struct mm_struct *mm = current->mm;

    // 检查用户内存中是否能够访问指定的程序名称区域
    // user_mem_check 函数用于验证内存是否可访问
    if (!user_mem_check(mm, (uintptr_t)name, len, 0)) {
        return -E_INVAL;  // 如果不可访问，则返回无效错误
    }

    // 限制程序名称长度，不超过 PROC_NAME_LEN
    if (len > PROC_NAME_LEN) {
        len = PROC_NAME_LEN;  // 如果程序名称长度超过最大限制，截断至最大长度
    }

    // 创建一个本地的程序名称数组，并初始化为 0
    char local_name[PROC_NAME_LEN + 1];
    memset(local_name, 0, sizeof(local_name));

    // 将传入的程序名称复制到本地数组
    memcpy(local_name, name, len);

    // 如果当前进程存在内存管理结构体（即该进程有自己的地址空间）
    if (mm != NULL) {
        // 输出调试信息
        cputs("mm != NULL");

        // 切换到内核的页目录，准备释放当前进程的内存空间
        lcr3(boot_cr3);

        // 减少该进程的内存引用计数
        if (mm_count_dec(mm) == 0) {
            // 如果该进程的内存计数为 0，表示没有其他进程使用这个地址空间
            // 执行内存回收操作
            exit_mmap(mm);    // 释放映射的内存
            put_pgdir(mm);    // 清除页目录
            mm_destroy(mm);   // 销毁内存管理结构体
        }

        // 将当前进程的内存管理结构体设置为 NULL
        current->mm = NULL;
    }

    int ret;
    // 加载新的程序（即二进制文件），并初始化新的内存空间
    // load_icode 函数会将二进制程序加载到内存中
    if ((ret = load_icode(binary, size)) != 0) {
        // 如果加载程序失败，执行错误退出
        goto execve_exit;
    }

    // 成功加载新程序后，设置当前进程的名称为传入的程序名称
    set_proc_name(current, local_name);
    return 0;  // 返回 0，表示成功

execve_exit:
    // 如果加载程序失败，调用 do_exit 退出进程
    do_exit(ret);
    // 进入 panic，表示程序执行失败
    panic("already exit: %e.\n", ret);
}


// do_yield - ask the scheduler to reschedule
int
do_yield(void) {
    current->need_resched = 1;
    return 0;
}

// do_wait - 等待一个或任何状态为 PROC_ZOMBIE 的子进程，并释放该子进程的内核栈的内存空间
//         - 释放该子进程的 proc 结构体。
// 注意：只有在 do_wait 函数执行后，子进程的所有资源才会被释放。
int
do_wait(int pid, int *code_store) {
    struct mm_struct *mm = current->mm;
    if (code_store != NULL) {
        if (!user_mem_check(mm, (uintptr_t)code_store, sizeof(int), 1)) {
            return -E_INVAL;
        }
    }

    struct proc_struct *proc;
    bool intr_flag, haskid;
repeat:
    haskid = 0;
    if (pid != 0) {
        proc = find_proc(pid);
        if (proc != NULL && proc->parent == current) {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE) {
                goto found;
            }
        }
    }
    else {
        proc = current->cptr;
        for (; proc != NULL; proc = proc->optr) {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE) {
                goto found;
            }
        }
    }
    if (haskid) {
        current->state = PROC_SLEEPING;
        current->wait_state = WT_CHILD;
        schedule();
        if (current->flags & PF_EXITING) {
            do_exit(-E_KILLED);
        }
        goto repeat;
    }
    return -E_BAD_PROC;

found:
    if (proc == idleproc || proc == initproc) {
        panic("wait idleproc or initproc.\n");
    }
    if (code_store != NULL) {
        *code_store = proc->exit_code;
    }
    local_intr_save(intr_flag);
    {
        unhash_proc(proc);
        remove_links(proc);
    }
    local_intr_restore(intr_flag);
    put_kstack(proc);
    kfree(proc);
    return 0;
}

// do_kill - kill process with pid by set this process's flags with PF_EXITING
int
do_kill(int pid) {
    struct proc_struct *proc;
    if ((proc = find_proc(pid)) != NULL) {
        if (!(proc->flags & PF_EXITING)) {
            proc->flags |= PF_EXITING;
            if (proc->wait_state & WT_INTERRUPTED) {
                wakeup_proc(proc);
            }
            return 0;
        }
        return -E_KILLED;
    }
    return -E_INVAL;
}

// kernel_execve - do SYS_exec syscall to exec a user program called by user_main kernel_thread
//kernel_execve 负责内核级的程序加载，
static int
kernel_execve(const char *name, unsigned char *binary, size_t size) {
    int64_t ret=0, len = strlen(name);
 //   ret = do_execve(name, len, binary, size);
    asm volatile(
        "li a0, %1\n"
        "lw a1, %2\n"
        "lw a2, %3\n"
        "lw a3, %4\n"
        "lw a4, %5\n"
    	"li a7, 10\n"
        "ebreak\n"
        "sw a0, %0\n"
        : "=m"(ret)
        : "i"(SYS_exec), "m"(name), "m"(len), "m"(binary), "m"(size)
        : "memory");
    cprintf("ret = %d\n", ret);
    return ret;
}
//帮助内核加载并执行用户程序的二进制文件
#define __KERNEL_EXECVE(name, binary, size) ({                          \
            cprintf("kernel_execve: pid = %d, name = \"%s\".\n",        \
                    current->pid, name);                                \
            kernel_execve(name, binary, (size_t)(size));                \
        })
//通过 KERNEL_EXECVE 宏加载并执行名为 x 的用户程序
#define KERNEL_EXECVE(x) ({                                             \
            extern unsigned char _binary_obj___user_##x##_out_start[],  \
                _binary_obj___user_##x##_out_size[];                    \
            __KERNEL_EXECVE(#x, _binary_obj___user_##x##_out_start,     \
                            _binary_obj___user_##x##_out_size);         \
        })
//类似于 __KERNEL_EXECVE，但是该宏更加通用，允许外部提供更灵活的二进制数据和大小参数。
#define __KERNEL_EXECVE2(x, xstart, xsize) ({                           \
            extern unsigned char xstart[], xsize[];                     \
            __KERNEL_EXECVE(#x, xstart, (size_t)xsize);                 \
        })

#define KERNEL_EXECVE2(x, xstart, xsize)        __KERNEL_EXECVE2(x, xstart, xsize)
/*
KERNEL_EXECVE2(TEST, TESTSTART, TESTSIZE) 的作用：
KERNEL_EXECVE2 是一个宏，用于通过内核函数 kernel_execve 执行一个用户程序。
该宏接受三个参数：TEST 是程序名（字符串），TESTSTART 和 TESTSIZE 分别是程序二进制文件的起始地址和大小。
KERNEL_EXECVE2 会调用 __KERNEL_EXECVE 宏，其中传入的是程序名称（#x）和二进制内容的指针及大小。
__KERNEL_EXECVE 宏负责打印当前进程的 PID 和程序名，然后调用 kernel_execve 执行实际的加载和运行操作。
TESTSTART 和 TESTSIZE 在外部被定义，应该是一个用户程序的二进制数据的起始位置和大小。
*/
// user_main - kernel thread used to exec a user program
/*加载：使用 KERNEL_EXECVE 系列宏，程序的二进制数据会在内核态被加载到内存中。这个加载过程是由内核完成的。
在程序加载完成后，内核通过上下文切换机制将执行控制权交给用户程序。此时，程序开始在用户态运行。系统调用、I/O 操作等将使程序和内核进行交互，但用户程序本身的执行始终是在用户态进行的。*/
static int
user_main(void *arg) {
//根据 TEST 是否被定义来决定执行的行为
#ifdef TEST
    KERNEL_EXECVE2(TEST, TESTSTART, TESTSIZE);
#else
    KERNEL_EXECVE(exit);
#endif//条件编译指令块的结束
    panic("user_main execve failed.\n");
}

// init_main - the second kernel thread used to create user_main kernel threads
static int
init_main(void *arg) {
    size_t nr_free_pages_store = nr_free_pages();
    size_t kernel_allocated_store = kallocated();

    int pid = kernel_thread(user_main, NULL, 0);
    if (pid <= 0) {
        panic("create user_main failed.\n");
    }

    while (do_wait(0, NULL) == 0) {
        schedule();
    }

    cprintf("all user-mode processes have quit.\n");
    assert(initproc->cptr == NULL && initproc->yptr == NULL && initproc->optr == NULL);
    assert(nr_process == 2);
    assert(list_next(&proc_list) == &(initproc->list_link));
    assert(list_prev(&proc_list) == &(initproc->list_link));

    cprintf("init check memory pass.\n");
    return 0;
}

// proc_init - set up the first kernel thread idleproc "idle" by itself and 
//           - create the second kernel thread init_main
void
proc_init(void) {
    int i;

    list_init(&proc_list);
    for (i = 0; i < HASH_LIST_SIZE; i ++) {
        list_init(hash_list + i);
    }

    if ((idleproc = alloc_proc()) == NULL) {
        panic("cannot alloc idleproc.\n");
    }

    idleproc->pid = 0;
    idleproc->state = PROC_RUNNABLE;
    idleproc->kstack = (uintptr_t)bootstack;
    idleproc->need_resched = 1;
    set_proc_name(idleproc, "idle");
    nr_process ++;

    current = idleproc;

    int pid = kernel_thread(init_main, NULL, 0);
    if (pid <= 0) {
        panic("create init_main failed.\n");
    }

    initproc = find_proc(pid);
    set_proc_name(initproc, "init");

    assert(idleproc != NULL && idleproc->pid == 0);
    assert(initproc != NULL && initproc->pid == 1);
}

// cpu_idle - at the end of kern_init, the first kernel thread idleproc will do below works
void
cpu_idle(void) {
    while (1) {
        if (current->need_resched) {
            schedule();
        }
    }
}

