# Lab0.5

## 实验0.5主要讲解最小可执行内核和启动流程。我们的内核主要在 Qemu 模拟器上运行，它可以模拟一台 64 位 RISC-V 计算机。为了让我们的内核能够正确对接到 Qemu 模拟器上，需要了解 Qemu 模拟器的启动流程，还需要一些程序内存布局和编译流程（特别是链接）相关知识

## 练习1: 使用GDB验证启动流程

### 为了熟悉使用qemu和gdb进行调试工作,使用gdb调试QEMU模拟的RISC-V计算机加电开始运行到执行应用程序的第一条指令（即跳转到0x80200000）这个阶段的执行过程，说明RISC-V硬件加电后的几条指令在哪里？完成了哪些功能？要求在报告中简要写出练习过程和回答

### 首先保证可以正确运行Lab0.5，测试结果如下

![[alt text](f55b101701e727d51372f24dcf4fb6d.png)](https://github.com/zoygk/myimage/blob/main/NKUOS/oslab/d2032edac40f98f6ca05a009468b784.png)

### 使用GDB进行调试，首先在终端中使用命令make debug，然后再make gdb运行结果如下

![d2032edac40f98f6ca05a009468b784.png](https://github.com/zoygk/myimage/blob/main/NKUOS/oslab/f55b101701e727d51372f24dcf4fb6d.png)

### 加电后执行的指令

![[alt text](ca9fdbc1f090b4bb4adf8a4a11e034d.png)](https://github.com/zoygk/myimage/blob/main/NKUOS/oslab/ca9fdbc1f090b4bb4adf8a4a11e034d.png)

### 其中指令0x1000:auipc t0 , 0x0将当前程序计数器PC中保存的值与0x0相加，并将结果保存在寄存器t0中，这里t0将获得当前PC值的高20位，单步调试，并查看t0寄存器的值，验证结果

![[alt text](20be9b113cae9e02b37580d5e507da6.png)](https://github.com/zoygk/myimage/blob/main/NKUOS/oslab/20be9b113cae9e02b37580d5e507da6.png)

### 可以看到r0寄存器的值为0x1000

### 第二条指令0x1004: addi a1, t0, 32将寄存器t0中的值与立即数32相加，并将结果存储在寄存器a1中，单步调试，验证结果

![[alt text](628d7ad32f68deb74bc28a65dae005e.png)](https://github.com/zoygk/myimage/blob/main/NKUOS/oslab/628d7ad32f68deb74bc28a65dae005e.png)

### 指令csrr a0, mhartid：mhartid是RISC-V的机器级CSR寄存器，用于存储当前硬件线程ID，本条指令用于从mhartid中读取硬件线程ID并将结果保存在a0寄存器中，单步调试，查看寄存器a0的值，为0x0

![[alt text](27ce54906c079c38c6ff98a1a97cec7.png)](https://github.com/zoygk/myimage/blob/main/NKUOS/oslab/27ce54906c079c38c6ff98a1a97cec7.png)

### 指令ld t0, 24(t0)：用于从存储器中加载一个64位的值存储在寄存器t0中，目标地址为当前t0寄存器中的值加上偏移量24，单步调试，查看寄存器t0的值

![[alt text](377ef0bb70690da41fd82fb3b1635ae.png)](https://github.com/zoygk/myimage/blob/main/NKUOS/oslab/377ef0bb70690da41fd82fb3b1635ae.png)

### 指令jr t0：无条件跳转到寄存器t0中存储的地址处，即跳转到0x80000000处，同时后面的指令unimp是一个保留指令，通常用于指示未实现的功能或作为占位符。如果执行到这条指令，处理器会触发一个异常，即后面的指令无意义

### 单步调试，执行指令x/10i 0x80000000查看0x80000000处的指令，该处加载的是QEMU的bootloader —— OpenSBI.bin，其作用是加载操作系统的内核并启动操作系统的执行，该处指令如下

![[alt text](a9b8eec1333830242c392b9228dc7d0.png)](https://github.com/zoygk/myimage/blob/main/NKUOS/oslab/a9b8eec1333830242c392b9228dc7d0.png)

### 同样的，该处使用si逐步调试，info r查看寄存器的值，其中指令csrr，auipc，addi的作用上面已经介绍过，在此不再赘述，指令bgtz即“Branch if Greater Than Zero”的缩写，大于0则跳转到目的地址，指令sd t1, 0(t0)将 t1 寄存器的值存储到 t0 寄存器指向的内存地址（t0作为基地址，偏移量为0），指令ld t0, 0(t0)从 t0 寄存器指向的内存地址加载一个64位的值到t0寄存器

![[alt text](9b6db77f9f32d446c0c870b5e8c824f.png)](https://github.com/zoygk/myimage/blob/main/NKUOS/oslab/9b6db77f9f32d446c0c870b5e8c824f.png)

### 根据实验指导书可知， OpenSBI启动之后将要跳转到的一段汇编代码：kern/init/entry.S，在这里进行内核栈的分配，而这段汇编代码的地址是固定的0x80200000，在此处设置断点

![[alt text](1d48d50794ae03e09dd572f15b1052d.png)](https://github.com/zoygk/myimage/blob/main/NKUOS/oslab/1d48d50794ae03e09dd572f15b1052d.png)

### 使用指令x/10i 0x80200000查看此处汇编代码，可以发现在0x80200008处，程序跳转到了kern_init

![[alt text](340a57a3b711a2e8c7c4e1af5061d70.png)](https://github.com/zoygk/myimage/blob/main/NKUOS/oslab/340a57a3b711a2e8c7c4e1af5061d70.png)

### 然后输入continue执行直到断点，make debug终端输出，同时在gdb终端执行了一个 la 指令，用于将栈指针寄存器 sp 设置为 bootstacktop 的值

![[alt text](6d24acee25faeeb113c034193807937.png)](https://github.com/zoygk/myimage/blob/main/NKUOS/oslab/6d24acee25faeeb113c034193807937.png)

### 为了对kern_init进行分析，接着输入break kern_init设置断点，执行到断点处查看反汇编代码，通过分析反汇编代码，最后一条指令总是跳转到自己，故进入死循环中，再次continue查看make debug终端输出

![[alt text](720c80bf325c710e66825564585437e.png)](https://github.com/zoygk/myimage/blob/main/NKUOS/oslab/720c80bf325c710e66825564585437e.png)
