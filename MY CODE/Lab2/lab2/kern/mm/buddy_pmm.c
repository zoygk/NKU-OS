#include <pmm.h>
#include <list.h>
#include <string.h>
#include <buddy_pmm.h>
#include <stdio.h>

#define MAX_ORDER 15 // 最大阶次 2^10 = 1024 pages

typedef struct {
    list_entry_t free_list[MAX_ORDER + 1];  // 每个阶次的空闲链表
    size_t nr_free[MAX_ORDER + 1];  // 当前空闲页面的数量
} free_area2_t;

free_area2_t free_area2;  // 全局空闲区域
static uintptr_t min_addr;

#define free_list free_area2.free_list
#define nr_free free_area2.nr_free

static void
buddy_init(void) {
    for (int order = 0; order <= MAX_ORDER; order++) {
        list_init(&free_list[order]);  // 初始化每个链表
        nr_free[order] = 0;            // 设置空闲块数量为 0
    }
}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    // 将所有页面标记为保留状态
    struct Page *p = base;
    for (; p != base + n; p++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }

    // 将整个内存区域标记为空闲
    size_t order = 0; // 初始化块的大小
    while ((1 << order) < n) order++; // 找到适当的块大小

    base->property = 1 << order; // 将块大小记录在第一个页面中
    SetPageProperty(base); // 将此页标记为已分配
    min_addr = page2pa(base);

    // 初始化自由列表和数量
    for (size_t j = 0; j <= MAX_ORDER; j++) {
        list_init(&free_list[j]);
        nr_free[j] = 0;
    }

    // 将第一个块加入适当的自由列表
    list_add(&free_list[order], &(base->page_link));
    nr_free[order]++; // 记录这个块的数量
}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);
    int order = 0;
    while ((1 << order) < n) order++;  // 找到能满足需求的最小块

    for (int current_order = order; current_order <= MAX_ORDER; current_order++) {
        if (!list_empty(&free_list[current_order])) {
            list_entry_t *le = list_next(&free_list[current_order]);
            struct Page *page = le2page(le, page_link);
            list_del(le);
            nr_free[current_order]--;

            // 如果块较大，分裂
            while (current_order > order) {
                current_order--;
                struct Page *buddy_page = page + (1 << current_order);
                buddy_page->property = 1 << current_order;
                SetPageProperty(buddy_page);
                list_add(&free_list[current_order], &(buddy_page->page_link));
                nr_free[current_order]++;
            }

            ClearPageProperty(page);

            return page;
        }
    }
    return NULL;  // 没有足够大的块可分配
}

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

static size_t
buddy_nr_free_pages(void) {
    size_t total_free = 0;
    for (size_t order = 0; order <= MAX_ORDER; order++) {
        total_free += nr_free[order] * (1 << order); // 计算每种块的总数
    }
    return total_free; // 返回总空闲页面数量
}

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

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};