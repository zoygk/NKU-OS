# Lab2

## 练习一：理解first-fit 连续物理内存分配算法

first-fit 连续物理内存分配算法作为物理内存分配一个很基础的方法，需要同学们理解它的实现过程。请大家仔细阅读实验手册的教程并结合kern/mm/default_pmm.c中的相关代码，认真分析default_init，default_init_memmap，default_alloc_pages， default_free_pages等相关函数，并描述程序在进行物理内存分配的过程以及各个函数的作用。 请在实验报告中简要说明你的设计实现过程。请回答如下问题：

- 你的first fit算法是否有进一步的改进空间？

### first-fit算法

#### 算法思想

First-fit 算法的核心思想是：在分配物理内存时，总是选择第一个满足条件的内存块进行分配，而不考虑它是否是最优的选择。只要找到的内存块大小不小于请求的大小，就会被分配。

#### default_init

该函数负责初始化空闲内存块列表（free_list）以及空闲内存块的计数器（nr_free）。先调用 list_init(&free_list)来初始化空闲块链表。
再将 nr_free 设置为 0，表示目前没有可用的空闲内存块。

#### default_init_memmap

函数default_init_memmap初始化一段连续的物理页面，并将其添加到空闲页面列表中，从而确保这些页面在内存管理系统中被正确标识和管理。。详细注释如下：

```c
static void
     default_init_memmap(struct Page *base, size_t n) {
    // 确保请求的页面数量大于0
    assert(n > 0);
    // 从base开始的指针p，用于遍历将要初始化的页面
    struct Page *p = base;

    /* 遍历从base开始的n个页面，每次遍历确保页面被标记为已保留（未使用），并将页面的标志和属性设置为0，同时设置页面的引用计数为0 */
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }

    // 设置第一个页面的属性为n，表示它代表n个连续的页面
    base->property = n;

    // 将第一个页面的属性标记为有效的空闲页面。
    SetPageProperty(base);

    // 更新全局的空闲页面计数
    nr_free += n;

    // 如果空闲列表为空，将base页面添加到空闲列表中
    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        // 如果空闲列表不为空，查找插入位置
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            // 如果base页面在当前页面之前，插入到当前节点之前
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            // 否则将base页面添加到列表末尾
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }
}
```

#### default_alloc_pages

default_alloc_pages函数从空闲页面列表中分配连续的n个空闲页。如果找到足够的连续空闲页面，则更新空闲列表和页面属性，并返回分配的页面。如果未找到足够的页面，则返回 NULL。

```c
static struct Page *
default_alloc_pages(size_t n) {
    assert(n > 0);
    // 如果请求的页面数量大于当前可用的空闲页面数量，返回NULL
    if (n > nr_free) {
        return NULL;
    }
    // 有足够的连续空闲页面
    // 初始化页面指针为NULL
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    // 遍历空闲列表，寻找满足条件的空闲页面
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        // 如果当前页面的属性（表示可用的页面数）大于等于请求的数量
        if (p->property >= n) {
            // 记录找到的页面
            page = p;
            break;
        }
    }

    // 如果找到合适的页面
    if (page != NULL) {
        // 获取当前页面的前一个列表节点
        list_entry_t* prev = list_prev(&(page->page_link));
        // 从空闲列表中删除找到的页面
        list_del(&(page->page_link));
        // 如果找到的页面比请求的数量大
        if (page->property > n) {
            // 创建一个指向剩余页面的指针
            struct Page *p = page + n;
            // 更新剩余页面的属性
            p->property = page->property - n;
            SetPageProperty(p);
            // 将剩余页面添加回空闲列表
            list_add(prev, &(p->page_link));
        }
        // 更新可用页面数量
        nr_free -= n;
        // 清除已分配页面的属性标志
        ClearPageProperty(page);
    }
    return page;
}
```

#### default_free_pages

函数default_free_pages释放内存空间,即将指定数量的页面释放并返回到空闲列表中。同时可以的话合并相邻的空闲页面，以减少空闲列表中小块的数量。

```c
static void
default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    // 清除被释放内存块中所有页框的标志和属性信息，将引用计数置零
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }

    // 设置基础页面的属性为释放的页面数量
    base->property = n;
    // 标记基础页面为有效的空闲页面
    SetPageProperty(base);
    // 更新可用页面数量
    nr_free += n;

    // 如果空闲列表为空，将基础页面添加到空闲列表
    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        // 从空闲列表的头开始遍历,找到合适的位置将释放的内存块插入free_list。
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }

    // 尝试合并低地址的空闲页面
    list_entry_t* le = list_prev(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        // 如果低地址页面的末尾和基础页面相邻
        if (p + p->property == base) {
            /* 合并页面属性，从空闲列表中删除基础页面，并更新基础页面为合并后的页面 */
            p->property += base->property;
            ClearPageProperty(base);
            list_del(&(base->page_link));
            base = p;
        }
    }

    // 尝试合并高地址的空闲页面
    le = list_next(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
    }
}
```

#### 物理内存分配过程

1、内存初始化 (default_init 和 default_init_memmap)<br>

default_init:

- 初始化空闲页面链表 free_list。
- 将可用页面数 nr_free 设为 0。

default_init_memmap(struct Page *base, size_t n):

- 确定可用的物理页面 base，并将它们的状态设置为可用。
- 遍历指定的页面范围，确保每个页面是保留的，清除它们的标志，并将引用计数设为 0。
- 设置 base 页面的属性为 n（即释放的页面数量）。
- 更新全局可用页面计数 nr_free。
- 将 base 页面添加到空闲页面链表中，保持链表的有序性。

2、分配页面 (default_alloc_pages)<br>

default_alloc_pages(size_t n):

- 请求分配至少 n 个物理页面。
- 首先检查请求的页面数 n 是否大于当前可用页面 nr_free，如果是，则返回 NULL，表示无法满足请求。
- 遍历空闲页面链表 free_list，查找具有足够属性（可用页面数）的页面。
- 如果找到合适的页面，将其从链表中移除。
- 如果页面的属性大于请求的页面数 n，则将剩余的页面数量更新，并添加剩余页面到空闲链表中。
- 更新全局可用页面计数 nr_free，减少已分配页面的数量，并返回分配的页面的起始指针。

3、释放页面 (default_free_pages)<br>

default_free_pages(struct Page *base, size_t n):

- 请求释放 n 个物理页面，指向 base。
- 检查请求的页面数 n 是否大于 0。
- 清除每个页面的状态，确保它们不是保留的并且没有被标记为已分配。
- 更新 base 页面的属性为 n，并设置为可用。
- 增加可用页面计数 nr_free。
- 将 base 页面插入到空闲页面链表中，确保链表的有序性。
- 检查并尝试合并与相邻的空闲页面（前后），以减少内存碎片。

#### 各个函数的作用

default_init:
负责初始化物理内存管理的全局变量和数据结构，包括设置可用页面计数为 0，并初始化空闲页面链表。

default_init_memmap:
接收可用的物理页面和数量信息，设置页面状态，更新属性，增加可用页面计数，并将页面添加到空闲链表。

default_alloc_pages:
负责实际的物理页面分配。它检查请求的页面数量是否可用，并从空闲链表中分配页面，同时更新链表和可用页面计数。

default_free_pages:
负责释放页面并将其返回到空闲链表。它更新页面状态，检查相邻页面的合并条件，以减少内存碎片，并维护链表的有序性。

#### 算法的改进空间

- 当前实现的 default_free_pages 函数已尝试合并相邻的空闲块，但这一过程可以进一步优化，使用更高效的策略来检测和合并相邻空闲块，进而以减少内存碎片。
- 目前使用的是线性搜索，这可能在空闲块较多时导致性能瓶颈。可以考虑使用更高效的数据结构（如链表的有序版本、平衡树或位图）来管理空闲块，以加速查找过程。
- 可以根据分配请求的大小和历史请求的分布，动态调整策略。比如，针对较小的请求使用小块的分配器，而针对较大的请求使用大块的分配器。
- 实现一个延迟释放机制，在一定条件下（例如内存使用率较高时）暂时不释放空闲块，等待更合适的释放时机，以减少频繁的内存分配和释放操作。

## 练习二：实现 Best-Fit 连续物理内存分配算法

在完成练习一后，参考kern/mm/default_pmm.c对First Fit算法的实现，编程实现Best Fit页面分配算法，算法的时空复杂度不做要求，能通过测试即可。 请在实验报告中简要说明你的设计实现过程，阐述代码是如何对物理内存进行分配和释放，并回答如下问题：

- 你的 Best-Fit 算法是否有进一步的改进空间？

### Best-Fit算法

实现Best-Fit算法的时候，参考default_pmm.c中的算法，根据实验指导手册的提示依次完成各个函数即可。

#### best_fit_init_memmap

在best_fit_init_memmap，参照default_pmm.c中的First Fit中的efault_init_memmap进行初始化。

```c
    for (; p != base + n; p ++) {
        assert(PageReserved(p));

        /*LAB2 EXERCISE 2: YOUR CODE*/ 
        // 清空当前页框的标志和属性信息，并将页框的引用计数设置为0
        p->flags =0;
        p->property = 0;    // 清空
        set_page_ref(p, 0); //set_page_ref 设置页面引用计数为0
    }
```

```c
while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
             /*LAB2 EXERCISE 2: YOUR CODE*/ 
            // 编写代码
            // 1、当base < page时，找到第一个大于base的页，将base插入到它前面，并退出循环
            // 2、当list_next(le) == &free_list时，若已经到达链表结尾，将base插入到链表尾部
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            }  
            if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
}
```

#### best_fit_alloc_pages

Best-Fit算法的核心思想是找到最佳匹配的空闲内存块，以最大程度地减少内存碎片。算法的核心就在这个函数里。

```c
/*LAB2 EXERCISE 2: YOUR CODE*/ 
    // 下面的代码是first-fit的部分代码，请修改下面的代码改为best-fit
    // 遍历空闲链表，查找满足需求的空闲页框
    // 如果找到满足需求的页面，记录该页面以及当前找到的最小连续空闲页框数量
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        // 既要满足n个大小的内存页框大小，也要保证大小是最小的，即Best-Fit
        if (p->property >= n&& p->property <min_size) {
            page = p;
            min_size=p->property; //更新最小内存块大小
        }
    }

    if (page != NULL) {
        list_entry_t* prev = list_prev(&(page->page_link));
        list_del(&(page->page_link));
        if (page->property > n) {
            struct Page *p = page + n;
            p->property = page->property - n;
            SetPageProperty(p);
            list_add(prev, &(p->page_link));
        }
        nr_free -= n;
        ClearPageProperty(page);
    }
```

#### best_fit_free_pages

函数best_fit_free_pages释放内存空间。

```c
/*LAB2 EXERCISE 2: YOUR CODE*/ 
    // 编写代码
    // 具体来说就是设置当前页块的属性为释放的页块数、并将当前页块标记为已分配状态、最后增加nr_free的值
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
```

```c
 if (le != &free_list) {
        p = le2page(le, page_link);
        /*LAB2 EXERCISE 2: YOUR CODE*/ 
         // 编写代码
        // 1、判断前面的空闲页块是否与当前页块是连续的，如果是连续的，则将当前页块合并到前面的空闲页块中
        // 2、首先更新前一个空闲页块的大小，加上当前页块的大小
        // 3、清除当前页块的属性标记，表示不再是空闲页块
        // 4、从链表中删除当前页块
        // 5、将指针指向前一个空闲页块，以便继续检查合并后的连续空闲页块
        if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            list_del(&(base->page_link));
            base = p;
        }
    }
```

#### 物理内存分配过程

该算法的内存分配过程与First-Fit类似，主要区别在于分配页面时既要找到满足n个大小的内存页框，同时该内存页框大小也要保证是空闲链表中最小的。别的地方都类似。

#### 实验结果

实验结果如下：
![alt text](https://github.com/zoygk/myimage/blob/main/NKUOS/Lab2/1.png)
![alt text](https://github.com/zoygk/myimage/blob/main/NKUOS/Lab2/2.png)

#### 进一步的改进空间

- 在 best_fit_free_pages 函数中，可以在合并前后检查合并的结果，确保合并后的空闲块不超过一定大小，避免形成过大的空闲块导致内存碎片化。
- 在添加和删除链表节点时，可以使用更高效的方法来维护链表，减少链表遍历的复杂度，比如使用指针直接指向链表的头部和尾部，避免每次都从头遍历。
- 可以考虑实现更复杂的内存分配策略，比如使用组合最佳适应和首次适应的方法，在内存不足时优先使用最佳适应，确保分配效率和内存使用率。

## Challenge1：buddy system（伙伴系统）分配算法

#### Buddy System算法把系统中的可用存储空间划分为存储块(Block)来进行管理, 每个存储块的大小必须是2的n次幂(Pow(2, n)), 即1, 2, 4, 8, 16, 32, 64, 128...参考伙伴分配器的一个极简实现，在ucore中实现buddy system分配算法，要求有比较充分的测试用例说明实现的正确性，需要有设计文档。  
  
设计文档：  

#### 数据结构

free_area2_t：该结构体管理不同阶次的空闲块。free_list是一个链表数组，每个阶次对应一个链表，用于存储相应阶次的空闲内存块。nr_free则记录了每个阶次的空闲块数量。

```c
typedef struct {
    list_entry_t free_list[MAX_ORDER + 1];  // 每个阶次的空闲链表
    size_t nr_free[MAX_ORDER + 1];  // 当前空闲页面的数量
} free_area2_t;
```

#### 核心功能

1、buddy_init()：初始化伙伴系统，清空所有阶次的空闲块链表，并将每个阶次的空闲块数量设为0。

```c
static void
buddy_init(void) {
    for (int order = 0; order <= MAX_ORDER; order++) {
        list_init(&free_list[order]);  // 初始化每个链表
        nr_free[order] = 0;            // 设置空闲块数量为 0
    }
}
```

2、buddy_init_memmap()：初始化指定范围的内存页面为可用状态，找到合适的阶次，将内存块插入对应的链表，并更新空闲块数量。  

逻辑：
- 先将所有页面标记为保留（未使用）状态。
- 计算块的大小，并将其标记为对应阶次的块。
- 将该块插入相应阶次的空闲链表。

```c
static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    // 遍历从base开始的n个页面，每次遍历确保页面被标记为已保留（未使用），并将页面的标志和属性设置为0，同时设置页面的引用计数为0
    struct Page *p = base;
    for (; p != base + n; p++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }

    size_t order = 0; // 初始化块的大小
    while ((1 << order) < n) order++; // 找到适当的块大小

    base->property = 1 << order; // 将块大小记录在第一个页面中
    SetPageProperty(base); // 将此页标记为已分配
    min_addr = page2pa(base);  // 记录最低地址

    // 初始化自由列表和数量
    for (size_t j = 0; j <= MAX_ORDER; j++) {
        list_init(&free_list[j]);
        nr_free[j] = 0;
    }

    // 将第一个块加入适当的自由列表
    list_add(&free_list[order], &(base->page_link));
    nr_free[order]++; // 记录这个块的数量
}
```

3、buddy_alloc_pages()：分配满足请求大小的内存块。如果找到的块比需求大，会进行分裂，将多余的部分重新插入相应的空闲链表。

```c
static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);  // 确保请求的页数大于0
    int order = 0;
    
    // 计算满足请求n页的最小阶次（order），2^order >= n
    while ((1 << order) < n) order++;  // 每增加一阶，块大小翻倍

    // 从请求的阶次开始，遍历所有可能的阶次，直到最大阶次
    for (int current_order = order; current_order <= MAX_ORDER; current_order++) {
        // 如果当前阶次有空闲块，进入处理逻辑
        if (!list_empty(&free_list[current_order])) {
            list_entry_t *le = list_next(&free_list[current_order]);  // 获取空闲块的第一个元素
            struct Page *page = le2page(le, page_link);  // 获取该链表项对应的Page结构
            list_del(le);  // 从空闲链表中删除该块
            nr_free[current_order]--;  // 更新该阶次的空闲块计数

            // 如果找到的块大于请求大小，进行块的分裂
            while (current_order > order) {
                current_order--;  // 逐步降低阶次，分裂成更小的块
                struct Page *buddy_page = page + (1 << current_order);  // 找到分裂后的伙伴块
                buddy_page->property = 1 << current_order;  // 设置分裂后的块大小
                SetPageProperty(buddy_page);  // 将分裂出的伙伴块标记为有property的空闲块
                list_add(&free_list[current_order], &(buddy_page->page_link));  // 将伙伴块加入空闲链表
                nr_free[current_order]++;  // 更新该阶次的空闲块计数
            }

            ClearPageProperty(page);  // 清除原始块的property标志，表示它不再是空闲块

            return page;  // 返回满足请求的块
        }
    }
    return NULL;  // 如果没有足够大的块可用，返回NULL
}
```

4、buddy_free_pages()：释放指定的内存块并尝试与相邻的块合并。若相邻块也为空闲，合并成一个更大的块，并插入相应的阶次链表。

```c
static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0);

    struct Page *p = base;
    for (; p != base + n; p++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }

    int order = 0;
    while ((1 << order) < n) order++;  // 找到块的大小

    struct Page *buddy_page;
    uintptr_t buddy_addr;
    uintptr_t base_addr;

    while (order < MAX_ORDER) {
        //计算伙伴块的地址以找到伙伴块
        base_addr = page2pa(base);
        if (((base_addr - min_addr) / (1 << (order + 12))) % 2 == 0) {
            buddy_addr = base_addr + (1 << (order + 12));
        } else {
            buddy_addr = base_addr - (1 << (order + 12));
        }
        buddy_page = pa2page(buddy_addr);
        if (!PageProperty(buddy_page)) {
            // 伙伴块不空闲，不能合并
            break;
        }

        // 从链表中删除伙伴块，合并两个块
        list_del(&(buddy_page->page_link));
        nr_free[order]--;  // 减少空闲块计数

        if (buddy_page < base) {
            base = buddy_page;  // 使 page 指向合并后的块
        }

        ClearPageProperty(buddy_page);
        order++;  // 增加块大小
    }

    base->property = 1 << order;
    SetPageProperty(base);

    // 将合并后的块插入适当大小的链表
    list_add(&free_list[order], &(base->page_link));
    nr_free[order]++;  // 增加空闲块计数
}
```

5、buddy_nr_free_pages()：返回所有阶次的空闲页面总数。

```c
static size_t
buddy_nr_free_pages(void) {
    size_t total_free = 0;
    for (size_t order = 0; order <= MAX_ORDER; order++) {
        total_free += nr_free[order] * (1 << order); // 计算每种块的总数
    }
    return total_free; // 返回总空闲页面数量
}
```

6、buddy_check()：通过一系列分配和释放操作，验证伙伴系统的正确性，包括内存块的合并和分裂。

```c
static void
buddy_check(void) {
    int order = 0;
    struct Page *p0, *p1, *p2, *p3, *p4;

    // 记录初始的空闲页总数
    unsigned int nr_free_store = nr_free_pages();
    cprintf("Initial nr_free_pages: %u\n", nr_free_store);

    // Step 1: 分配多个不同大小的内存块
    cprintf("Step 1: Allocating pages...\n");
    p0 = alloc_pages(1); // 分配 1 页
    p1 = alloc_pages(2); // 分配 2 页
    p2 = alloc_pages(4); // 分配 4 页
    p4 = alloc_pages(129);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    cprintf("p0: %p (1 page), p1: %p (2 pages), p2: %p (4 pages)\n", p0, p1, p2);
    cprintf("p4: %p (129 page)\n", p4);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);
    assert(page2pa(p4) < npage * PGSIZE);
    //检测分配的块的位置关系是否符合伙伴系统特性
    assert(p0 + 2 == p1 && p0 + 4 == p2);
    assert(p0 + 256 == p4);

    // 检查是否分配了正确大小的页
    assert(!PageProperty(p0) && !PageProperty(p1) && !PageProperty(p2));
    assert(!PageProperty(p4));
    cprintf("Pages allocated successfully, all pages are valid.\n");

    // Step 2: 释放并检测是否合并空闲块
    cprintf("Step 2: Freeing pages and checking merge...\n");
    free_pages(p0, 1);  // 释放 1 页
    free_pages(p1, 2);  // 释放 2 页
    free_pages(p2, 4);  // 释放 4 页
    free_pages(p4, 129);

    // 检查释放后总的空闲页数是否恢复
    unsigned int nr_free_after_free = nr_free_pages();
    cprintf("nr_free_pages after free: %u\n", nr_free_after_free);
    assert(nr_free_after_free == nr_free_store);
    cprintf("Freeing and merging successful, free pages restored.\n");

    // Step 3: 再次分配，确保可以正确分配出已经释放的块
    cprintf("Step 3: Re-allocating pages...\n");
    p3 = alloc_pages(4);  // 再次分配 4 页
    assert(p3 != NULL);
    cprintf("p3: %p (re-allocated 4 pages)\n", p3);

    // 检查是否分配了正确大小的页
    assert(!PageProperty(p3));
    cprintf("Re-allocation successful, pages are valid.\n");

    // Step 4: 释放并检测空闲页的更新情况
    cprintf("Step 4: Freeing re-allocated pages...\n");
    free_pages(p3, 4);  // 释放 4 页
    unsigned int nr_free_after_realloc = nr_free_pages();
    cprintf("nr_free_pages after re-allocation free: %u\n", nr_free_after_realloc);
    assert(nr_free_after_realloc == nr_free_store);
    cprintf("Freeing re-allocated pages successful, free pages restored.\n");

    // Step 5: 检查伙伴合并情况 (e.g., 释放相邻的块时进行合并)
    cprintf("Step 5: Allocating adjacent pages and checking merge...\n");
    p0 = alloc_pages(2);  // 分配 2 页
    p1 = alloc_pages(2);  // 再次分配 2 页
    assert(p0 != NULL && p1 != NULL);
    cprintf("p0: %p (2 pages), p1: %p (2 pages)\n", p0, p1);

    free_pages(p0, 2);  // 释放 2 页
    free_pages(p1, 2);  // 释放相邻的 2 页

    // 检查释放后是否合并为更大的块
    unsigned int nr_free_after_merge = nr_free_pages();
    cprintf("nr_free_pages after merging adjacent blocks: %u\n", nr_free_after_merge);
    assert(nr_free_after_merge == nr_free_store);
    cprintf("Merging adjacent blocks successful.\n");

    cprintf("All steps in basic_check completed successfully!\n");
}
```

#### 流程描述
- 初始化：调用buddy_init()初始化伙伴系统。
- 内存初始化：使用buddy_init_memmap()将内存页块初始化为可分配状态，并插入相应的空闲链表。
- 内存分配：使用buddy_alloc_pages()从空闲链表中分配适当大小的内存块。若没有正好匹配的块，分裂较大的块以满足需求。
- 内存释放：使用buddy_free_pages()释放内存块并尝试与相邻的空闲块合并。
- 检测：通过buddy_check()验证内存块的合并、释放操作是否正确。

#### 运行结果
如下图所示，可以看到，伙伴系统可以正常运行并通过了check函数的测试。
![buddy result](https://github.com/zoygk/myimage/blob/main/NKUOS/Lab2/3.png)

## Challenge3：硬件的可用物理内存的获取方法

#### 1.在Linux系统中，可以通过读取特定文件信息的方式获取可用物理内存信息：

1. /proc/meminfo：通过读取**/proc/meminfo**文件来获取当前系统的内存信息，    /proc/meminfo是一个虚拟文件，提供了系统的内存信息。该文件由内核维护，包含了系统的内存状态，包括可用的物理内存范围。  当操作系统读取/proc/meminfo文件时，会得到一个文本文件，其中包含了以下信息：
   
       MemTotal: 总内存大小（包括物理内存和交换空间）
       MemFree: 可用的物理内存大小
       MemAvailable: 可用的物理内存大小（不包括缓存和缓冲区）
       Buffers: 缓冲区大小
       Cached: 缓存大小
       通过解析这些信息，操作系统可以获取到可用的物理内存范围。

2. /proc/iomem：    /proc/iomem也是一个虚拟文件，提供了系统的内存映射信息。该文件由内核维护，包含了系统的内存地址空间的映射信息。    当操作系统读取/proc/iomem文件时，会得到一个文本文件，其中包含了以下信息：
   
       iomem_: 内存地址空间的映射信息，包括起始地址、结束地址和映射类型（如RAM、ROM等）

3. sysfs：通过读取**/sys/devices/system/memory**目录下的文件来获取当前可用的物理内存范围信息：    在Linux系统中，/sys/devices/system/memory是一个 sysfs 文件系统，提供了系统的更加详细的内存信息。该文件系统由内核维护，包含了系统的内存设备信息，    操作系统读取/sys/devices/system/memory文件时，会得到一个文本文件，包含有内存设备和可用的物理内存大小相关信息。

操作系统首次启动时，内核的数据来源于硬件设备驱动（如BIOS、DMI或ACPI）的内存信息、内核自身内存管理模块以及可能的引导时加载的内存初始化模块。这些源头的信息被内核整合并转化为上述虚拟文件的内容，之后便以读取文件的方式给操作系统其他部分和用户空间提供内存使用情况。

#### 2.借助一些检测工具：在启动时，操作系统可以借助内存检测工具，如memtest86+或Prime95等，来检测系统的内存信息，包括可用的物理内存范围，以memtest86为例：

memtest86会从内存中选择一块来执行一系列的测试，以检测内存中的任何错误或故障。这些测试包括：

- 写和读测试: memtest86 将在内存中写入特定模式的数据，然后检查这些数据是否被正确读出，来验证内存的正确性。

- 模式测试: memtest86 会使用不同的模式（例如 0s、1s 和交替模式）来测试内存是否可以正确处理不同的数据类型。 

- 压力测试: memtest86 会执行大量的内存操作，以制造内存压力，检测未在正常条件下出现的故障。

#### 3. 读取BIOS/UEFI的信息来获取可用的物理内存范围：

BIOS/UEFI程序：BIOS（基本输入/输出系统）和UEFI（可扩展固件接口）都包含一个程序，负责管理计算机硬件的初始化和配置。这个程序通常在计算机启动时执行：

1. **硬件检测**：BIOS/UEFI程序会检测计算机硬件的类型和配置，包括CPU、主板、内存、硬盘等。它会读取硬件的标识信息，如型号、版本、ID码等。
2. **物理内存检测**：检测计算机的物理内存，包括RAM（随机存取内存）和ROM（只读内存）。BIOS/UEFI程序会读取内存的大小、速度、类型等信息。
3. **内存映射**：建立一个内存映射表，记录每个内存块的起始地址、大小和类型。这个映射表用于后续的内存管理。
4. **系统分配**：根据系统的需求分配内存空间，通常分为系统模块、应用程序模块和用户数据区等。
5. **信息保存**：保存系统配置信息和内存映射表到其固件设置中，不需要用户干预就可以持续保留此信息。
6. **系统启动**：当系统启动时，BIOS/UEFI程序会载入，用户可以通过BIOS/UEFI设置程序查看和设置系统的配置信息。
