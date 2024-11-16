#include <defs.h>
#include <riscv.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_lru.h>
#include <list.h>

list_entry_t lru_pra_list_head;
//初始化LRU链表
static int
_lru_init_mm(struct mm_struct *mm)
{     
     list_init(&lru_pra_list_head); // 初始化LRU链表的头部
     mm->sm_priv = &lru_pra_list_head; // 将 mm_struct 的私有数据指向LRU链表
     return 0;
}
//在页面被映射为可交换的页面时，更新该页面的访问顺序。
static int
_lru_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    list_entry_t *head=(list_entry_t*) mm->sm_priv; // 获取LRU队列的头部
    list_entry_t *entry=&(page->pra_page_link); // 获取页面的队列节点
 
    assert(entry != NULL && head != NULL);
    
    list_add(head, entry);

    return 0;
}
//选择一个页面进行置换，即从 LRU 队列中选择最久未使用的页面（位于链表尾部的页面）并将其交换出去。
static int
_lru_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page, int in_tick)
{
    list_entry_t *head=(list_entry_t*) mm->sm_priv; // 获取 LRU 队列的头部
    assert(head != NULL);
    assert(in_tick==0);
    
    list_entry_t* entry = list_prev(head); // 获取队列尾部的页面，即最久未访问的页面
    if (entry != head) { // 如果队列非空
        list_del(entry); //从 LRU 队列中删除 entry（即最久未访问的页面）
        *ptr_page = le2page(entry, pra_page_link); // 将该页面赋给 ptr_page
    } else {
        *ptr_page = NULL; // 没有页面可以交换出去
    }
    return 0;
}

static int
_lru_init(void) //初始化的时候什么都不做
{
    return 0;
}

static int
_lru_set_unswappable(struct mm_struct *mm, uintptr_t addr)
{
    return 0;
}

static int
_lru_tick_event(struct mm_struct *mm)
{ 
    list_entry_t *head=(list_entry_t*) mm->sm_priv;

    list_entry_t *le = head;
    while ((le = list_next(le)) != head) {
        struct Page* page = le2page(le, pra_page_link);
        pte_t *ptep = get_pte(mm->pgdir, page->pra_vaddr, 0);

        // 如果页面被访问（PTE_A 位被置位）
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

struct swap_manager swap_manager_lru =
{
     .name            = "lru swap manager",
     .init            = &_lru_init,
     .init_mm         = &_lru_init_mm,
     .tick_event      = &_lru_tick_event,
     .map_swappable   = &_lru_map_swappable,
     .set_unswappable = &_lru_set_unswappable,
     .swap_out_victim = &_lru_swap_out_victim,
     .check_swap      = &_lru_check_swap,
};
