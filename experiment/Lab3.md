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
   实现FIFO页面置换的核心算法，选择一个被替换的页面（即最先进入内存的页面）并将其从队列中移除。
9. swap_map_swappable:  
   用于将页面加入相应的链表，并设置页面可交换。
10. assert():  
    用于逐步验证程序执行的正确性。在页面置换过程中，assert被用来确保不同阶段的页面访问次数与预期一致，帮助检查置换是否发生。
11. tlb_invalidate()：  
    刷新tlb。

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

> 补充完成do_pgfault（mm/vmm.c）函数，给未被映射的地址映射上物理页。设置访问权限 的时候需要参考页面所在 VMA 的权限，同时需要注意映射物理页时需要操作内存控制 结构所指定的页表，而不是内核的页表。
> 
> 请在实验报告中简要说明你的设计实现过程。请回答如下问题：
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

> 通过之前的练习，相信大家对FIFO的页面替换算法有了更深入的了解，现在请在我们给出的框架上，填写代码，实现 Clock页替换算法（mm/swap_clock.c）。(提示:要输出curr_ptr的值才能通过make grade)  
> 
> 请在实验报告中简要说明你的设计实现过程。请回答如下问题：
> 
> - 比较Clock页替换算法和FIFO算法的不同。

```c
// 将页面page插入到页面链表pra_list_head的末尾
    list_add_before(head, entry);
    //list_add(head,entry);

   // 将页面的visited标志置为1，表示该页面已被访问
    uintptr_t pageptr=page->pra_vaddr;
    pte_t *ptep = get_pte(mm->pgdir, pageptr, 0);//获得page对应的页表项
    if((*ptep & PTE_A)==0){
        *ptep=*ptep | PTE_A;//PTE_A=0x040=0b000001000000
    }

   page->visited=1;
    return 0;
```

**设计思路**：clock算法需要在页面链表中循环查找满足可以被换出的页面，所以有一个list指针curr_ptr用来遍历链表，每当curr_ptr指向的页面的访问标志是1，那么就换出这个页；访问标志是Page结构体中的visited变量=0/1，遍历链表时，如果visited标志是1，则置为0，并继续访问下一个节点，直到出现标志=0的页面。所以这里向链表插入新页面 （ 函数swap_map_swappable）时，节点顺序会对之后的内存访问换出的页面造成影响：如果是list_add_after,那么节点顺序是head→4→3→2→1，如果是liat-add_before,那么节点顺序是1→2→3→4→head。

**比较Clock页替换算法和FIFO算法**:Clock算法在FIFO基础上进行改进，FIFO算法没有考虑到页面的使用情况，完全按照页面分配的先后顺序进行替换，CLOCK算法中，如果页面被使用了，会在访问标志位设为1，那么下一个循环这个页就不会被换出，因为按照局部性原理，被使用的内存地址周围可能在不久后被再次使用，所以CLOCK减少了可能的页面交换。
1. 初始化
```c
static int
_clock_init_mm(struct mm_struct *mm)
{     
     /*LAB3 EXERCISE 4: YOUR CODE*/ 
     // 初始化pra_list_head为空链表
    list_init(&pra_list_head);
     // 初始化当前指针curr_ptr指向pra_list_head，表示当前页面替换位置为链表头
     curr_ptr=&pra_list_head;
     // 将mm的私有成员指针指向pra_list_head，用于后续的页面替换算法操作
     mm->sm_priv = &pra_list_head;
     //cprintf(" mm->sm_priv %x in fifo_init_mm\n",mm->sm_priv);
     return 0;
}
```
2. 设置页面可访问
```c
static int
_clock_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    list_entry_t *entry=&(page->pra_page_link);
    list_entry_t *head=(list_entry_t*) mm->sm_priv;
    assert(entry != NULL && curr_ptr != NULL);
    //record the page access situlation
    /*LAB3 EXERCISE 4: YOUR CODE*/ 
    // link the most recent arrival page at the back of the pra_list_head qeueue.
    // 将页面page插入到页面链表pra_list_head的末尾
    list_add_before(head, entry);
    //list_add(head,entry);
    //record the page access situlation
    //(1)link the most recent arrival page at the back of the pra_list_head qeueue.
    // 将页面的visited标志置为1，表示该页面已被访问
    
    uintptr_t pageptr=page->pra_vaddr;
    pte_t *ptep = get_pte(mm->pgdir, pageptr, 0);//获得page对应的页表项
    if((*ptep & PTE_A)==0){
        *ptep=*ptep | PTE_A;
    }
   page->visited=1;
    return 0;
}
```
3. 页面替换算法
```c
static int
_clock_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page, int in_tick)
{
     list_entry_t *head=(list_entry_t*) mm->sm_priv;
         assert(head != NULL);
     assert(in_tick==0);
     /* Select the victim */
     //(1)  unlink the  earliest arrival page in front of pra_list_head qeueue
     //(2)  set the addr of addr of this page to ptr_page
    while (1) {
        /*LAB3 EXERCISE 4: YOUR CODE*/ 
        // 编写代码
        // 遍历页面链表pra_list_head，查找最早未被访问的页面
        if(curr_ptr==head){
            curr_ptr=list_next(curr_ptr);
            continue;
        }
        // 获取当前页面对应的Page结构指针
        struct Page *p = le2page(curr_ptr, pra_page_link);
        pte_t *ptep = get_pte(mm->pgdir, p->pra_vaddr, 0);
        //cprintf("ptep now = 0x%x\n",*ptep);
        // 如果当前页面未被访问，则将该页面从页面链表中删除，并将该页面指针赋值给ptr_page作为换出页面
        //if((p->visited)==0){
            if(!(*ptep & PTE_A)){
            cprintf("curr_ptr 0xffffffff%x\n",curr_ptr);
            list_del(curr_ptr);
            *ptr_page = p;
            curr_ptr=list_next(curr_ptr); 
            break;
        }else{ // 如果当前页面已被访问，则将visited标志置为0，表示该页面已被重新访问

            //*ptep = *ptep + PTE_A;
            *ptep=((*ptep)>>8)<<8|(*ptep&0xbf);
            p->visited=0;
            //更新curr_ptr
            curr_ptr=list_next(curr_ptr);
        }
            
    }
    return 0;
}
```
**注释**:在判断页面是否被访问时有两种方式，一个是根据Page结构体的visited成员，另一个是通过Page的pra_vaddr（虚拟地址）找到对应的页表项，根据页表项里的Access位判断



## 练习5：阅读代码和实现手册，理解页表映射方式相关知识（思考题）

> 如果我们采用”一个大页“ 的页表映射方式，相比分级页表，有什么好处、优势，有什么坏处、风险？

- 好处和优势
1. 一个大页包含原本的512个页，使用大页可以减少页表条目的数量，因为每个页表条目可以映射更多的物理内存。这可以减少页表本身占用的内存，从而提高内存利用率。

2. 提高TLB命中率和减少页表缓存失效：由于页表条目数量减少，翻译后备缓冲器（TLB）中的条目可以覆盖更大的地址空间，这可能提高TLB的命中率，减少页表查找的时间。

3. 提高内存分配效率：对于需要大量连续内存的应用（如数据库和大数据分析），大页可以更快地分配大块内存，因为操作系统不需要将多个小页合并来满足请求。
- 坏处和劣势
1. 内存碎片和利用率下降：如果大页不被充分利用，可能会导致更多的内存浪费，因为每个大页占用的内存比小页多。

2. 灵活性降低：大页减少了页表的粒度，这可能降低了内存分配的灵活性，尤其是在内存需求多样化的环境中。

3. 页面换入换出的代价增加：尽管一个大页包含了比一个页更多的内容，减少了页面交换次数，但是出现了页面缺失后从磁盘换入换出页面的时间会增加，因为一个页的数据增加了。

## 扩展练习 Challenge：实现不考虑实现开销和效率的LRU页替换算法

#### 分析

最久未使用(least recently used, LRU)算法：利用局部性，通过过去的访问情况预测未来的访问情况，我们可以认为最近还被访问过的页面将来被访问的可能性大，而很久没访问过的页面将来不太可能被访问。于是我们比较当前内存里的页面最近一次被访问的时间，把上一次访问时间离现在最久的页面置换出去。  

为了实现LRU算法，容易想到的一种做法是，每当硬件访问一个页面时，将该页面在LRU队列中的位置移至队列头，这样，队列尾部即为最久未访问的页面。但是，现有的硬件并不支持这种做法，因为硬件只有在访问一个会触发PageFault的页面时，swap_manager才会收到信息进而修改LRU队列，而硬件在访问一个不会触发PageFault的页面时，swap_manager不会收到任何信息，也就不会执行任何操作。我们能得到的唯一的硬件支持是在硬件访问一个页时，该页的PTE_A位会被置位，而且这个置位也是没有任何通知的，只有当操作系统主动去读取该位时，才会知道这个页曾经被访问过。  

在只有这一硬件支持的情况下，为了尽可能地贴合LRU算法的思想，考虑让swap_manager周期性（如时钟中断）的去读取LRU链表中每个页的PTE_A位，将被置位的页移至队列头，并将它们的PTE_A位复位。读取的周期越短，这一实现就越接近LRU算法。

#### 设计

由于只有在周期性的中断时才会根据访问时间去更新LRU链表，只需要在_lru_tick_event（）函数中设计一个简单的更新算法即可，而_lru_init_mm（）、_lru_map_swappable（）、_lru_swap_out_victim（）这几个函数与FIFO算法没有区别。

```c
static int
_lru_tick_event(struct mm_struct *mm) //时钟中断时更新LRU队列
{ 
    list_entry_t *head=(list_entry_t*) mm->sm_priv;

    list_entry_t *le = head;
    //遍历LRU队列查找PTE_A 位被置位的页
    while ((le = list_next(le)) != head) {
        struct Page* page = le2page(le, pra_page_link);
        pte_t *ptep = get_pte(mm->pgdir, page->pra_vaddr, 0);

        // 如果页面被访问（PTE_A 位被置位），将该结点移至队列头
        if ((*ptep & PTE_A) != 0) {
            list_entry_t *entry=le;
            le = list_prev(entry);
            list_del(entry);
            list_add(head,entry);

            // 清除 PTE_A 位，表示该页面的访问状态被重置
            *ptep &= ~PTE_A;
        }
    }

    return 0; 
}
```

设计一个_lru_check_swap（）函数以检验算法的正确性，由于实际上并未实现时钟中断操作，在_lru_check_swap（）函数中主动调用swap_tick_event（）函数模拟时钟中断。

```c
static int
_lru_check_swap(void) {
    extern struct mm_struct *check_mm_struct;
    //队列（头->尾）：dcba
    swap_tick_event(check_mm_struct);
    //队列（头->尾）：abcd
    cprintf("write Virt Page e in lru_check_swap\n");
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num==5);
    swap_tick_event(check_mm_struct);
    //队列（头->尾）：eabc
    cprintf("write Virt Page a in lru_check_swap\n");
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num==5);
    swap_tick_event(check_mm_struct);
    //队列（头->尾）：aebc
    cprintf("write Virt Page d in lru_check_swap\n");
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num==6);
    swap_tick_event(check_mm_struct);
    //队列（头->尾）：daeb
    cprintf("write Virt Page b in lru_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num==6);
    swap_tick_event(check_mm_struct);
    //队列（头->尾）：bdae
    cprintf("write Virt Page c in lru_check_swap\n");
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num==7); 
    swap_tick_event(check_mm_struct);
    //队列（头->尾）：cbda
    cprintf("write Virt Page a in lru_check_swap\n");
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num==7);
    swap_tick_event(check_mm_struct);
    //队列（头->尾）：acbd
    cprintf("write Virt Page e in lru_check_swap\n");
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num==8);
    swap_tick_event(check_mm_struct);
    //队列（头->尾）：eacb
    cprintf("write Virt Page d in lru_check_swap\n");
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num==9);
    swap_tick_event(check_mm_struct);
    //队列（头->尾）：deac
    return 0;
}
```

#### 测试

运行make qemu，可以发现该算法正确执行了check_swap（）函数。
![LRU测试](https://github.com/zoygk/myimage/blob/main/NKUOS/Lab3/LRU%E6%B5%8B%E8%AF%95.png)
