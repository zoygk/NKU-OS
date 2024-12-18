#ifndef __LIBS_UNISTD_H__
#define __LIBS_UNISTD_H__

#define T_SYSCALL           0x80

/* syscall number */
#define SYS_exit            1
#define SYS_fork            2
#define SYS_wait            3
#define SYS_exec            4
#define SYS_clone           5
#define SYS_yield           10
#define SYS_sleep           11
#define SYS_kill            12
#define SYS_gettime         17
#define SYS_getpid          18
#define SYS_brk             19
#define SYS_mmap            20
#define SYS_munmap          21
#define SYS_shmem           22
#define SYS_putc            30
#define SYS_pgdir           31

/* SYS_fork flags */
#define CLONE_VM            0x00000100  // set if VM shared between processes
#define CLONE_THREAD        0x00000200  // thread group

#endif /* !__LIBS_UNISTD_H__ */
/*这些宏定义后面的数字代表了系统调用编号（syscall number）。在操作系统中，系统调用（syscall）是用户程序和内核之间的接口。每个系统调用都有一个唯一的编号，这些编号通常被定义为常量，便于在代码中引用
这些宏定义的数字（如 SYS_exit、SYS_fork 等）确实会在 syscall 函数中使用，作为系统调用号传递给内核，内核根据这些数字来判断应该执行哪种系统调用。
*/
