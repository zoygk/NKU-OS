# Lab3实验报告

## 练习0：填写已有实验

> 本实验依赖实验2。请把你做的实验2的代码填入本实验中代码中有“LAB2”的注释相应部分。（建议手动补充，不要直接使用merge）

## 练习一：理解基于FIFO的页面替换算法

> 描述FIFO页面置换算法下，一个页面从被换入到被换出的过程中，会经过代码里哪些函数/宏的处理（或者说，需要调用哪些函数/宏），并用简单的一两句话描述每个函数在过程中做了什么？

### 流程

从分配开始：find_vma() → get_pte() ,  
如果返回NULL， 则使用alloc_page() 函数分配一个新的物理页面，如果没有可用页面，则可能触发页面换出：swap_out() → sm->swap_out_victim() → list_del() → swapfs_write() → free_page()  
否则就是页面换入：swap_in() → swapfs_read()

### 函数

1. do_pgfault()  
当系统发生缺页异常后，程序会将跳转到该函数进行缺页处理。该函数先判断出错的虚拟地址在 mm_struct 里是否可用。如果地址有效且页面不存在，函数使用 get_pte 获取页表项，并调用 pgdir_alloc_page 为新页面分配物理内存；如果页面在磁盘上，则通过 swap_in 加载页面。
2. find_vma()  
在给定的 mm中查找包含指定地址 addr 的vma，找不到就返回NULL。
3. get_pte()  
通过页目录（pgdir）获取对应虚拟地址（la）的页表项（pte_t）。如果页表项不存在，并且 create 标志为真，则会分配新页面并更新页表。
4. swap_in()  
将虚拟地址 addr 对应的页面从磁盘交换区加载到内存中。
5. swap_out()  
将进程的内存页面从内存交换到磁盘的交换区，从而释放空间给新的界面。
6. swapfs_read()  
用于将磁盘中的数据写入指定的内存页面。
7. swapfs_write()  
用于将页面写入磁盘。在这里由于需要换出页面，而页面内容如果被修改过那么就与磁盘中的不一致，所以需要将其重新写回磁盘。
8. _fifo_swap_out_victim()  
实现FIFO页面置换的核心算法，选择一个“受害者”页面（即最先进入内存的页面）并将其从队列中移除。
9. swap_map_swappable:  
用于将页面加入相应的链表，并设置页面可交换。
10. assert():  
用于逐步验证程序执行的正确性。在页面置换过程中，assert被用来确保不同阶段的页面访问次数与预期一致，帮助检查置换是否发生。

## 练习二 ：深入理解不同分页模式的工作原理

> get_pte()函数（位于kern/mm/pmm.c）用于在页表中查找或创建页表项，从而实现对指定线性地址对应的物理页的访问和映射操作。这在操作系统中的分页机制下，是实现虚拟内存与物理内存之间映射关系非常重要的内容。
>
> - get_pte()函数中有两段形式类似的代码， 结合sv32，sv39，sv48的异同，解释这两段代码为什么如此相像。
> - 目前get_pte()函数将页表项的查找和页表项的分配合并在一个函数里，你认为这种写法好吗？有没有必要把两个功能拆开？

### get_pte()函数中有两段形式类似的代码， 结合sv32，sv39，sv48的异同，解释这两段代码为什么如此相像

#### sv32，sv39，sv48

sv32: 使用两级页表结构，适用于 32 位虚拟地址空间。地址由两部分组成：页目录项（10 位）和页表项（10 位），加上 12 位偏移。  
sv39: 使用三级页表结构，适用于 39 位虚拟地址空间。地址分为 3 个页目录项（9 位、9 位和 9 位），加上 12 位偏移。  
sv48: 使用四级页表结构，适用于 48 位虚拟地址空间。地址分为 4 个页目录项（9 位、9 位、9 位和 9 位），加上 12 位偏移。  
而在这三种模式下，每个页表项的大小都是 64 位。这种一致性使得页表项结构和标志位保持统一。

#### 相似性

相似代码如下：

```c
pde_t *pdep1 = &pgdir[PDX1(la)];
if (!(*pdep1 & PTE_V)) {
    struct Page *page;
    if (!create || (page = alloc_page()) == NULL) {
        return NULL;
    }
    set_page_ref(page, 1);
    uintptr_t pa = page2pa(page);
    memset(KADDR(pa), 0, PGSIZE);
    *pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V);
}

pde_t *pdep0 = &((pde_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)];

if (!(*pdep0 & PTE_V)) {
    struct Page *page;
    if (!create || (page = alloc_page()) == NULL) {
        return NULL;
    }
    set_page_ref(page, 1);
    uintptr_t pa = page2pa(page);
    memset(KADDR(pa), 0, PGSIZE);
    *pdep0 = pte_create(page2ppn(page), PTE_U | PTE_V);
}
```

get_pte() 函数的目的是查找或创建对应于指定线性地址的页表项。函数中主要的工作流程如下：  
查找页目录项：首先，通过线性地址 la 计算出页目录项的索引 PDX1(la)。检查该目录项是否有效（即是否存在）。  
创建页目录项：如果目录项无效且需要创建（create 为 true），则分配新的物理页面，并将其映射到页目录项。  
查找页表项：通过目录项获取页表的基地址，再根据线性地址计算出页表项的索引 PDX0(la)。  
创建页表项：如果页表项无效，同样进行创建。  

sv32，sv39，sv48 三种模式在结构上都要求分级逐步访问页表，确保每一级都分配和初始化，而这种分级逻辑的页表层级间的主要差异仅在于页表基地址的变化和每一级索引的位宽不同，但是每一级页表的基本操作逻辑是一致的：检查当前页表项是否有效，如果无效则需要分配新的页，并将其初始化。这就导致了这两段代码的相似性。

### 目前get_pte()函数将页表项的查找和页表项的分配合并在一个函数里，你认为这种写法好吗？有没有必要把两个功能拆开

我认为这种写法好，并不需要将两个功能拆开。通常我们只会在获取页表项时遇到缺失的情况，才需要进行页表的创建。将查找和分配合并在同一个函数中，可以有效减少代码的重复性和函数调用的开销，降低代码的复杂度，使得整体逻辑更加清晰,也可以进一步的提高性能。

## 练习三：给未被映射的地址映射上物理页（需要编程）

>补充完成do_pgfault（mm/vmm.c）函数，给未被映射的地址映射上物理页。设置访问权限 的时候需要参考页面所在 VMA 的权限，同时需要注意映射物理页时需要操作内存控制 结构所指定的页表，而不是内核的页表。
>
>请在实验报告中简要说明你的设计实现过程。请回答如下问题：
>
> - 请描述页目录项（Page Directory Entry）和页表项（Page Table Entry）中组成部分对ucore实现页替换算法的潜在用处。
> - 如果ucore的缺页服务例程在执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情？
>   - 数据结构Page的全局变量（其实是一个数组）的每一项与页表中的页目录项和页表项有无对应关系？如果有，其对应关系是啥？

### 解答

补充完整后的do_pgfault（）函数如下：

```c
int
do_pgfault(struct mm_struct *mm, uint_t error_code, uintptr_t addr) {
    int ret = -E_INVAL;
    //try to find a vma which include addr
    struct vma_struct *vma = find_vma(mm, addr);

    pgfault_num++;
    //If the addr is in the range of a mm's vma?
    if (vma == NULL || vma->vm_start > addr) {
        cprintf("not valid addr %x, and  can not find it in vma\n", addr);
        goto failed;
    }

    uint32_t perm = PTE_U;
    if (vma->vm_flags & VM_WRITE) {
        perm |= (PTE_R | PTE_W);
    }
    addr = ROUNDDOWN(addr, PGSIZE);

    ret = -E_NO_MEM;

    pte_t *ptep=NULL;

    ptep = get_pte(mm->pgdir, addr, 1);  
    if (*ptep == 0) {
        if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL) {
            cprintf("pgdir_alloc_page in do_pgfault failed\n");
            goto failed;
        }
    } else {
        if (swap_init_ok) {
            struct Page *page = NULL;
            //自己编写的代码如下：
            swap_in(mm,addr,&page); //把从磁盘中得到的页放进内存中
            page_insert(mm->pgdir,page,addr,perm);//在页表中新增加一个映射，并且设置权限
            swap_map_swappable(mm,addr,page,1);//将该内存页设置为可交换
            page->pra_vaddr = addr；
        } else {
            cprintf("no swap_init_ok but ptep is %x, failed\n", *ptep);
            goto failed;
        }
   }
   ret = 0;
failed:
    return ret;
}
```

#### 请描述页目录项（Page Directory Entry）和页表项（Page Table Entry）中组成部分对ucore实现页替换算法的潜在用处

PDE和PTE最基本的作用是存储物理地址映射信息，即它们包含指向物理内存地址或下一级页表的指针，而其中的某些字段可以为实现页面替换算法提供重要的信息。

比如PDE和PTE包含权限位（如读、写、执行权限），这些权限对于操作系统保护内存不被非法访问至关重要。

再比如存在位（Present），表明相应的页面是否在物理内存中，从而决定是否需要加载页面；修改位（Dirty），表明页面自被加载以来是否被修改过，进而决定是否需要将修改过的页面写回磁盘。；访问位（Accessed），表明页面自上次清零以来是否被访问过，可以用来实现LRU算法，优先替换那些最久未访问的页面。这些位都可以帮助操作系统决定哪些页面最适合被替换。

#### 如果ucore的缺页服务例程在执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情

生成异常：硬件捕捉到无效的内存访问请求并触发缺页异常。

保存当前的CPU状态：在处理页访问异常之前，CPU会保存当前的执行状态，包括寄存器的值、程序计数器（PC）、标志寄存器等。这些状态将帮助操作系统在处理完异常后恢复程序的执行。

加载异常处理程序：控制权转移到异常处理程序（如本次实验中的缺页处理函数do_pgfault），该程序将解析错误信息，并尝试解决缺页问题。

#### 数据结构Page的全局变量（其实是一个数组）的每一项与页表中的页目录项和页表项有无对应关系？如果有，其对应关系是啥

PDE指向页表，而PTE指向具体的物理页面，映射到 Page 数组中的某一项。Page结构体所管理的物理页可以通过页表项间接关联。页表项存储物理地址信息，这又可以用来索引到对应的 Page 结构体，从而允许操作系统管理和跟踪物理内存的使用。

## 练习4：补充完成Clock页替换算法（需要编程）

>通过之前的练习，相信大家对FIFO的页面替换算法有了更深入的了解，现在请在我们给出的框架上，填写代码，实现 Clock页替换算法（mm/swap_clock.c）。(提示:要输出curr_ptr的值才能通过make grade)  
>
>请在实验报告中简要说明你的设计实现过程。请回答如下问题：
>
> - 比较Clock页替换算法和FIFO算法的不同。

## 练习5：阅读代码和实现手册，理解页表映射方式相关知识（思考题）

>如果我们采用”一个大页“ 的页表映射方式，相比分级页表，有什么好处、优势，有什么坏处、风险？
