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

```
static void
     default_init_memmap(struct Page *base, size_t n) {
    // 确保请求的页面数量大于0
    assert(n > 0);
    // 从base开始的指针p，用于遍历将要初始化的页面
    struct Page *p = base;

    /* 遍历从base开始的n个页面，每次遍历确保页面被标记为已保留（未使用），并将页面的标志和属性设置为0，同时给hi设置页面的引用计数为0 */
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

```
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

```
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

```
    for (; p != base + n; p ++) {
        assert(PageReserved(p));

        /*LAB2 EXERCISE 2: YOUR CODE*/ 
        // 清空当前页框的标志和属性信息，并将页框的引用计数设置为0
        p->flags =0;
        p->property = 0;    // 清空
        set_page_ref(p, 0); //set_page_ref 设置页面引用计数为0
    }
```

```
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

```
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

```
/*LAB2 EXERCISE 2: YOUR CODE*/ 
    // 编写代码
    // 具体来说就是设置当前页块的属性为释放的页块数、并将当前页块标记为已分配状态、最后增加nr_free的值
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
```

```
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
