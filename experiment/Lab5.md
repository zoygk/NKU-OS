# Lab5：用户程序

## 实验目的

- 了解第一个用户进程创建过程
- 了解系统调用框架的实现机制
- 了解ucore如何实现系统调用sys_fork/sys_exec/sys_exit/sys_wait来进行进程管理

## 练习0：填写已有实验

> 本实验依赖实验2/3/4。请把你做的实验2/3/4的代码填入本实验中代码中有“LAB2”/“LAB3”/“LAB4”的注释相应部分。注意：为了能够正确执行lab5的测试应用程序，可能需对已完成的实验2/3/4的代码进行进一步改进。

## 练习1: 加载应用程序并执行（需要编码）

> do_execv函数调用load_icode（位于kern/process/proc.c中）来加载并解析一个处于内存中的ELF执行文件格式的应用程序。你需要补充load_icode的第6步，建立相应的用户内存空间来放置应用程序的代码段、数据段等，且要设置好proc_struct结构中的成员变量trapframe中的内容，确保在执行此进程后，能够从应用程序设定的起始执行地址开始执行。需设置正确的trapframe内容。
>
> 请在实验报告中简要说明你的设计实现过程。
>
> - 请简要描述这个用户态进程被ucore选择占用CPU执行（RUNNING态）到具体执行应用程序第一条指令的整个经过。

### 代码

```c
    /* LAB5:EXERCISE1 YOUR CODE
     * should set tf->gpr.sp, tf->epc, tf->status
     */
    tf->gpr.sp = USTACKTOP;
    tf->epc = elf->e_entry;
    tf->status = sstatus & ~(SSTATUS_SPP | SSTATUS_SPIE);
```

### `load_icode`函数整体解析

`load_icode` 函数是操作系统中加载一个 ELF 格式二进制程序的函数。

- **检查当前进程的内存管理结构**

  - 如果当前进程已经有了内存管理结构 (`mm`)，说明该进程不允许被重新加载二进制程序，因此调用 `panic` 来报告错误。

- **创建新的内存管理结构**

  - 调用 `mm_create()` 创建一个新的内存管理结构（`mm`），用于管理当前进程的虚拟内存空间。
  - 如果内存不足或分配失败，跳转到错误处理部分。

- **设置页目录**

  - 调用 `setup_pgdir()` 创建一个新的页目录（PDT）并为当前进程配置。

- **加载 ELF 文件的程序头**

  - 通过解析 ELF 格式的头文件，读取程序头（`ph`），并检查 ELF 魔术数来验证文件格式的有效性。

- **加载各个段**

  - 遍历 ELF 文件中的每个程序段，根据程序头中的信息决定如何映射到进程的虚拟内存中。
  - 对于可执行的、写入的和读写的段，调用 `mm_map()` 为这些段分配虚拟内存并将它们映射到进程的内存空间中。
  - 如果段有实际数据（如 `.text`、`.data`），就将它们从 ELF 文件复制到分配的内存中。如果段是 BSS 段（未初始化的全局变量），则将其内存清零。

- **为用户栈分配内存**

  - 为用户栈分配足够的内存空间，并映射到进程的虚拟地址空间中。

- **设置当前进程的内存管理结构**

  - 将进程的内存管理结构（`mm`）和 CR3 寄存器指向新的页目录。

- **设置进程的 Trapframe**

  - `trapframe` 用于保存进程从内核切换到用户态时的寄存器状态。设置其中的栈指针、入口地址和状态寄存器，确保进程能够正确从内核返回到用户空间。

### 请简要描述这个用户态进程被ucore选择占用CPU执行（RUNNING态）到具体执行应用程序第一条指令的整个经过

流程如下：

1. 在`init_main`中通过`kernel_thread`调用`do_fork`创建并唤醒线程
2. 执行函数`user_main`，这时该线程状态已经为`PROC_RUNNABLE`，表明该线程开始运行
3. 在`user_main`中调用`kernel_execve`
4. 在`kernel_execve`中执行`ebreak`，发生断点异常跳转到`CAUSE_BREAKPOINT`
5. 在`CAUSE_BREAKPOINT`处调用`syscall`，执行`sys_exec`，调用`do_execve`
6. 在`do_execve`中调用`load_icode`，加载文件
7. 加载完毕后返回，直到`__alltraps`的末尾，接着执行`__trapret`后的内容，到`sret`，表示退出S态，回到用户态执行，这时开始执行用户的应用程序

## 练习2: 父进程复制自己的内存空间给子进程（需要编码）

> 创建子进程的函数do_fork在执行中将拷贝当前进程（即父进程）的用户内存地址空间中的合法内容到新进程中（子进程），完成内存资源的复制。具体是通过copy_range函数（位于kern/mm/pmm.c中）实现的，请补充copy_range的实现，确保能够正确执行。
>
> 请在实验报告中简要说明你的设计实现过程。
>
> 如何设计实现Copy on Write机制？给出概要设计，鼓励给出详细设计。
>
> - Copy-on-write（简称COW）的基本概念是指如果有多个使用者对一个资源A（比如内存块）进行读操作，则每个使用者只需获得一个指向同一个资源A的指针，就可以该资源了。若某使用者需要对这个资源A进行写操作，系统会对该资源进行拷贝操作，从而使得该“写操作”使用者获得一个该资源A的“私有”拷贝—资源B，可对资源B进行写操作。该“写操作”使用者对资源B的改变对于其他的使用者而言是不可见的，因为其他使用者看到的还是资源A。

### 代码

```c
            /* LAB5:EXERCISE2 YOUR CODE
             * (1) find src_kvaddr: the kernel virtual address of page
             * (2) find dst_kvaddr: the kernel virtual address of npage
             * (3) memory copy from src_kvaddr to dst_kvaddr, size is PGSIZE
             * (4) build the map of phy addr of  nage with the linear addr start
             */
            uintptr_t* src = page2kva(page);
            uintptr_t* dst = page2kva(npage);
            memcpy(dst, src, PGSIZE);
            ret = page_insert(to, npage, start, perm);
```

### `copy_range` 函数解释

这个函数的整体作用是将源进程的虚拟地址空间中的一段内存（按照页面对齐）复制到目标进程的虚拟地址空间中。

- **输入参数检查：**
  - 函数首先确认了 `start` 和 `end` 是页面对齐的地址（即它们的值应该是页大小 `PGSIZE` 的倍数）。如果不是，断言会失败。
  - `USER_ACCESS(start, end)` 确保 `start` 和 `end` 范围内的地址对用户进程是可访问的。

- **循环复制内存：**
  - 函数通过一个 `do-while` 循环按页复制内存，直到 `start` 大于或等于 `end`。

- **处理每一页：**
  - 对于每个页面，首先检查源进程的页表项是否有效。`get_pte(from, start, 0)` 函数用于获取源进程在 `start` 地址处的页表项。如果找不到页表项，则将 `start` 向上对齐到下一个页的起始位置，并继续。
  - 如果源页表项存在且有效，函数会在目标进程的页表中查找一个空白页表项，并分配物理页面。

- **分配新页面并复制数据：**
  - 函数为目标进程分配一个新的页面，并获取源进程对应页面的内核虚拟地址（`page2kva(page)`），然后将源页面的内容复制到目标页面。
  - 复制数据后，函数使用 `page_insert` 将新页面映射到目标进程的地址空间。

- **重复直到完成复制：**
  - 循环继续，直到复制整个地址范围。

- **返回结果：**
  - 如果成功复制所有页面，函数返回 `0`。

### 如何设计实现Copy on Write机制？给出概要设计

Copy-on-write（简称COW）的基本概念是指如果有多个使用者对一个资源A（比如内存块）进行读操作，则每个使用者只需获得一个指向同一个资源A的指针，就可以该资源了。若某使用者需要对这个资源A进行写操作，系统会对该资源进行拷贝操作，从而使得该“写操作”使用者获得一个该资源A的“私有”拷贝—资源B，可对资源B进行写操作。该“写操作”使用者对资源B的改变对于其他的使用者而言是不可见的，因为其他使用者看到的还是资源A。

#### 1、页表的共享与标记
- 共享页面：在进程创建时，不直接复制父进程的内存页，而是让子进程和父进程的页表指向同一物理内存页。
- 页表标记：将共享页面的权限设置为只读。使用页表中的标记位（如页表项的 R/W 位或额外的标志位）记录这些页面为 COW 页面。
#### 2、页面写时触发机制
- 写保护陷阱：如果父进程或子进程试图写入共享页面，CPU 会触发 页面保护异常。
- 异常处理：内核捕获页面保护异常，判断是否属于 COW 机制，如果是 COW 页面，进行实际的内存复制，将新的物理页设置为可写，并更新页表指针，最后恢复进程继续执行。
#### 3、内存复制机制
- 实际复制数据：为触发写操作的进程分配一个新的物理内存页，将原来共享页面的内容拷贝到新页面，然后更新进程的页表，使其指向新的物理页，并设置权限为可读写。
- 引用计数：每个共享页面维护一个引用计数，记录有多少进程共享该页面，当引用计数降为 0 时，释放该物理内存页。
#### 4、引用计数的管理
- 分配新页面时：如果页面被复制，减少原页面的引用计数。
- 进程结束时：当一个进程退出，释放其页表项并减少对应物理页面的引用计数。

## 练习3: 阅读分析源代码，理解进程执行 fork/exec/wait/exit 的实现，以及系统调用的实现（不需要编码）

> 请在实验报告中简要说明你对 fork/exec/wait/exit函数的分析。并回答如下问题：
>
> - 请分析fork/exec/wait/exit的执行流程。重点关注哪些操作是在用户态完成，哪些是在内核态完成？内核态与用户态程序是如何交错执行的？内核态执行结果是如何返回给用户程序的？
> - 请给出ucore中一个用户态进程的执行状态生命周期图（包执行状态，执行状态之间的变换关系，以及产生变换的事件或函数调用）。（字符方式画即可）  
>
> 执行：make grade。如果所显示的应用程序检测都输出ok，则基本正确。（使用的是qemu-1.0.1）

### 分析fork/exec/wait/exit的执行流程

#### 1、fork

#### 用户态

- 用户进程调用 fork 函数，进入用户态库函数syscall。
- 用户态库函数通过内联汇编指令将参数传给寄存器，并调用 ecall 指令触发中断切换到内核态进行中断处理。

#### 内核态

- 在中断处理时，首先修改 sepc 寄存器以再返回后跳过 ecall 指令，然后调用内核态系统调用函数 syscall。
- syscall 会取出寄存器中保存的参数，并调用相应的处理函数 sys_fork 处理系统调用。
- sys_fork调用do_fork函数初始化一个新线程并为新线程分配内核栈空间
- 为新线程分配新的虚拟内存或与其他线程共享虚拟内存
- 通过copy_mm函数复制进程的内存管理信息
- 获取原线程的上下文与中断帧，设置当前线程的上下文与中断帧将新线程插入哈希表和链表中，最终唤醒新线程并返回线程id
- 系统调用处理完成后，即中断处理结束，返回用户态。

#### 用户态
- 中断返回后，返回值通过内联汇编存储在变量ret中，返回ret，函数调用结束。

exec/wait/exit函数的执行流程与fork函数基本相同，区别只有传递的参数不同以及内核态中调用的处理函数不同。下面只介绍这几个函数核心处理函数的执行流程。
#### 2、exec
- 系统调用处理函数 sys_exec 使用用户态传递的四个参数（程序名称、长度、二进制数据及其大小）调用核心处理函数 do_exec。
- 检查程序是否在合法的用户地址空间内，且是否具有读权限。
- 限制进程名称的大小，防止超出最大进程名称长度。
- 将进程名称从用户地址空间复制到内核的局部缓冲区 local_name 中。
- 如果当前进程已经有内存空间，需要先释放旧的地址空间。
- 调用 load_icode 加载程序二进制数据到内存中。
- 更新当前进程的名称为 local_name，便于调试和管理。
- 如果发生错误，调用 do_exit 终止进程。
#### 3、wait
- 系统调用处理函数 sys_wait 使用用户态传递的两个参数（子进程PID及 code_store 指针）调用核心处理函数 do_wait。
- 检查用户传入的 code_store 指针是否有效，确保其指向合法的用户内存区域，并且具有写权限。
- 如果指定了 pid ，查找指定子进程是否存在，否则遍历进程列表，寻找任意处于 PROC_ZOMBIE 状态的子进程。
- 如果当前进程有子进程，但没有找到退出的子进程，将当前进程设置为 PROC_SLEEPING，并将 wait_state 标记为 WT_CHILD（等待子进程），之后调用 schedule 切换到其他可运行的进程。如果当前进程被杀死（PF_EXITING），直接退出。
- 如果找到了合适的子进程，首先判断是否是特殊进程，如果不是则继续执行，然后判断 code_store 是否为空，如果为空则将子进程的退出码写入 code_store，最后释放子进程。
#### 4、exit
- 系统调用处理函数 sys_exit 使用用户态传递的一个参数（进程退出码）调用核心处理函数 do_exit。
- 检查当前进程是否为idleproc或initproc，如果是，发出panic。
- 如果是用户进程，首先释放地址空间。
- 设置进程状态及退出码。
- 关闭中断，如果父进程处于等待子进程状态，则唤醒父进程，将当前进程的所有直接子进程重新分配到 initproc 的子进程链表中，开启中断。
- 调用调度器，选择新的进程执行。
### 用户态进程的执行状态生命周期图
```
                +----------------+
                |  PROC_UNINIT   |
                +----------------+
                        |
                do_fork() 初始化进程控制块
                        |
                +----------------+
                |  PROC_SLEEPING |
                +----------------+
                        |
                被唤醒（wakeup_proc()）
                        |
                +----------------+
                |  PROC_RUNNABLE |
                +----------------+
                        |
                schedule() 被调度
                        |
                +----------------+
      +-------->|  PROC_RUNNABLE | ----------------+
      |         +----------------+                 |
      |                 |                          |
 等待事件完成   被阻塞（wait()/I/O 等）             |
      |                 |                          |
      |         +----------------+                 |
      +-------- |  PROC_SLEEPING |          do_exit()/完成任务
                +----------------+                 |
                                                   |
                                                   |
                                                   |
                +----------------+                 |
                |   PROC_ZOMBIE  |<----------------+
                +----------------+
                        |
            被父进程回收 do_wait()
                        |
                +----------------+
                |  （进程结束）  |
                +----------------+
```

### 实验结果

![alt text](https://github.com/zoygk/myimage/blob/main/NKUOS/Lab5/1.png)

## 扩展练习 Challenge2

说明该用户程序是何时被预先加载到内存中的？与我们常用操作系统的加载有何区别，原因是什么？

### uCore 中用户程序的预加载
#### 1、预加载时机
uCore 是一个教学操作系统，用户程序通常在内核初始化期间预先加载到内存中。具体地，在系统启动时，内核通过文件系统接口（例如内置文件系统或内存映射的方式）将用户程序从简单的存储区域加载到内存中。
#### 2、加载方式
用户程序的二进制文件（如 ELF 格式）通常通过内核中的 load_icode 或类似函数加载。
load_icode 会解析用户程序的二进制格式，分配内存，设置页表，并将程序的代码段和数据段拷贝到物理内存的指定位置。
加载完成后，内核为用户程序创建一个进程控制块（proc_struct）并设置入口地址，准备将控制权转交给用户态程序。
### 常用操作系统中用户程序的预加载
#### 1、加载时机
在 Linux、Windows 等常见操作系统中，用户程序通常在用户请求执行时加载（如通过 exec 系统调用）。程序的加载由内核与用户态的动态链接器共同完成，程序仅在需要时加载，非预加载。
#### 2、加载方式
操作系统通过文件系统接口从磁盘或其他存储设备读取程序。ELF 文件解析通常由内核完成，内核为程序分配内存并设置页表。部分内容采用按需加载（lazy loading）方式，仅在程序访问时才加载到内存，减少启动时的资源消耗。动态链接器负责解析动态库，并将它们加载到进程地址空间中。
### uCore 设计的原因

- 1、教学目的：uCore 的目标是简化操作系统实现，为教学提供一个清晰易懂的框架。预加载减少了与复杂文件系统和硬件交互相关的代码量。
- 2、资源限制：uCore 通常运行在模拟器或简单硬件上，资源有限。通过预加载，可以避免复杂的内存管理和磁盘 I/O 操作。
- 3、功能限制：uCore 仅支持基础操作系统功能，不需要实现复杂的按需加载和动态链接。
- 4、调试方便：直接将用户程序嵌入内核镜像或内存，可以减少运行时的不确定性，方便开发和调试。
