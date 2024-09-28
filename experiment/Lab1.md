# Lab1 
#### 练习1：理解内核启动中的程序入口操作
**阅读 kern/init/entry.S内容代码，结合操作系统内核启动流程，说明指令 la sp, bootstacktop 完成了什么操作，目的是什么？ tail kern_init 完成了什么操作，目的是什么？**

la sp, bootstacktop：
操作：使用 la（Load Address）指令将 bootstacktop（内核堆栈的顶部） 的地址加载到堆栈指针 sp 中。  
目的：设置初始的堆栈环境。bootstacktop 通常标记着内核堆栈的顶部。通过将 sp 设置到 bootstacktop，内核初始化代码确定了堆栈位置，同时确保了堆栈从高地址向低地址增长，为内核进入C语言环境做了准备。

tail kern_init：
操作：tail 关键字表示这是一个尾部调用，它会将控制权转移给 kern_init 函数，并且当前的堆栈帧将被 kern_init 函数复用。  
目的：跳转到C语言的初始化函数执行初始化任务。

#### 练习2：完善中断处理
**编程完善trap.c中的中断处理函数trap，在对时钟中断进行处理的部分填写kern/trap/trap.c函数中处理时钟中断的部分，使操作系统每遇到100次时钟中断后，调用print_ticks子程序，向屏幕上打印一行文字”100 ticks”，在打印完10行后调用sbi.h中的shut_down()函数关机。**

完善后的代码如下：  
```c
case IRQ_S_TIMER:
    //调用定义好的函数clock_set_next_event设置下一次时钟中断
    clock_set_next_event();
    //计数器加一
    ticks++;
    //如果触发了100次时钟中断，则输出一个100ticks
    if(ticks % TICK_NUM == 0){
    cprintf("100ticks\n");
    //如果打印了10次，调用sbi_shutdown函数关机
    if(ticks / TICK_NUM == 10) sbi_shutdown();
    }
    break;
```
运行结果如下图所示：  
![](clock.png)  

#### 扩展练习 Challenge1：描述与理解中断流程
**描述ucore中处理中断异常的流程（从异常的产生开始），其中mov a0，sp的目的是什么？SAVE_ALL中寄存器保存在栈中的位置是什么确定的？对于任何中断，__alltraps 中都需要保存所有寄存器吗？请说明理由。**

中断异常处理流程：异常的产生->跳转到中断入口点->保存上下文->进入中断处理程序->执行中断处理->恢复上下文并返回  
各阶段具体描述如下：  
异常的产生：当 CPU 执行指令过程中检测到异常条件，比如非法指令、缺页等，会触发异常。异常发生时，CPU会自动将当前程序挂起，并开始运行对应的异常处理程序。  
跳转到中断入口点：CPU 会跳转到预先设定的中断入口点，通常是 __alltraps 函数。  
保存上下文：在进入异常处理程序之前，当前 CPU 状态（包括寄存器内容）会被保存在栈中，以便异常处理结束后可以恢复现场。
进入中断处理程序：通过指令mov a0，sp，将堆栈指针传给a0，作为参数调用中断处理函数 trap ，以便在处理程序中访问当前线程的堆栈信息。  
执行中断处理： trap 函数会根据不同的中断类型执行相应的中断处理程序。  
恢复上下文并返回：中断处理完成，从栈中恢复之前保存的 CPU 状态并通过 sret 指令返回中断发生时的指令。  
  
SAVE_ALL中寄存器保存在栈中的位置通过 sp 寄存器确定， sp 寄存器中保存的地址为寄存器结构体的基地址，结构体中依次排列通用寄存器 x0 到 x31 ，然后依次排列 sstatus ， sepc ， sbadvaddr ， scause 这4个和中断相关的 CSR ，通过 sp 寄存器及偏移量即可访问保存的寄存器。  
  
__alltraps 中不一定需要保存所有寄存器。如果中断处理程序不会改变太多的上下文信息，比如时钟中断，就可以只保存少量寄存器。

#### 扩增练习 Challenge2：理解上下文切换机制
**在trapentry.S中汇编代码 csrw sscratch, sp；csrrw s0, sscratch, x0实现了什么操作，目的是什么？SAVE_ALL里面保存了stval、scause这些csr，而在RESTORE_ALL里面却不还原它们，那这样restore的意义何在呢？**

csrw sscratch, sp：将当前的栈指针 sp 值写入 sscratch 寄存器。sscratch 是一个系统寄存器，用于在异常或中断处理期间临时存储数据。  
csrrw s0, sscratch, x0：首先将 sscratch 寄存器的值读取到 s0 寄存器中，然后将 x0 寄存器的值（x0 寄存器始终为0）写入到 sscratch 寄存器中。  
目的：在中断或异常发生时，保存当前状态的栈指针，以便在中断或异常处理完成后能够恢复到原来的状态。  
  
scause 用于记录异常或中断的原因，stval 用于提供额外的错误代码或状态信息。这些值通常在异常处理程序中被读取并用于确定发生了什么类型的异常或中断，以及需要采取什么行动。然而，它们的值只在处理过程中有用，一旦处理完成，它们的值就不再需要了。因此，在异常处理完成后，scause 和 stval 的值通常不需要恢复。

#### 扩展练习Challenge3：完善异常中断
**编程完善在触发一条非法指令异常 mret 后，在 kern/trap/trap.c的异常处理函数中捕获，并对其进行处理，简单输出异常类型和异常指令触发地址，即“Illegal instruction caught at 0x(地址)”，“ebreak caught at 0x（地址）”与“Exception type:Illegal instruction"，“Exception type: breakpoint”。**

异常处理代码如下：  
```c
case CAUSE_ILLEGAL_INSTRUCTION:
    //输出异常指令地址
    cprintf("Illegal instruction caught at 0x%08x\n",tf->epc);
    //输出指令异常类型
    cprintf("Exception type:Illegal instruction\n");
    //进行简单的处理，通过更新tf->epc寄存器的值跳过该异常指令
    tf->epc += 4;
    break;
case CAUSE_BREAKPOINT:
    //输出异常指令地址
    cprintf("ebreak caught at 0x%08x\n",tf->epc);
    //输出指令异常类型
    cprintf("Exception type:breakpoint\n");
    //进行简单的处理，通过更新tf->epc寄存器的值跳过该异常指令
    tf->epc += 2;
    break;
```
在kern_init函数中嵌入两条非法汇编指令触发异常：  
```c
int kern_init(void) {
    extern char edata[], end[];
    memset(edata, 0, end - edata);

    cons_init();

    const char *message = "(THU.CST) os is loading ...\n";
    cprintf("%s\n\n", message);

    print_kerninfo();

    idt_init();

    clock_init();
    
    intr_enable();
    
    //使用ebreak指令触发一个断点中断，从而进入中断处理流程
    __asm__("ebreak");
    //使用.word将0x00000000插入到二进制文件中，由于该指令的操作码为0000000，属于无效指令，从而触发中断，进入中断处理流程
    __asm__(".word 0x00000000");

    while (1);
}
```  
运行结果如下图所示：  
![](mret.png)