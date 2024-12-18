#include <defs.h>
#include <unistd.h>
#include <stdarg.h>
#include <syscall.h>

#define MAX_ARGS            5

static inline int syscall(int num, ...) {
    // va_list, va_start, va_arg 都是 C 语言中处理不定数量参数的宏
    // 它们在 stdarg.h 中定义
    va_list ap; // ap: 参数列表（此时未初始化）
    va_start(ap, num); // 初始化参数列表，从 num 开始
    // 首先，va_start 初始化一个可变参数列表，作为 va_list 类型。
    uint64_t a[MAX_ARGS]; // 用于存储传入的所有参数
    int i, ret;
    for (i = 0; i < MAX_ARGS; i++) { // 将所有参数逐个取出
        /* 随后执行 va_arg 宏会返回按顺序传入的每个参数
           每次调用 va_arg 都会返回对应的下一个参数值。 */
        a[i] = va_arg(ap, uint64_t); // 取出参数，存储到 a 数组
    }
    va_end(ap); // 在函数返回之前，必须调用 va_end 来结束对可变参数的访问。

    // 使用内联汇编来执行系统调用
    asm volatile (
        "ld a0, %1\n"   // 加载参数 num 到 a0 寄存器
        "ld a1, %2\n"   // 加载参数 a[0] 到 a1 寄存器
        "ld a2, %3\n"   // 加载参数 a[1] 到 a2 寄存器
        "ld a3, %4\n"   // 加载参数 a[2] 到 a3 寄存器
        "ld a4, %5\n"   // 加载参数 a[3] 到 a4 寄存器
        "ld a5, %6\n"   // 加载参数 a[4] 到 a5 寄存器
        "ecall\n"       // 执行环境调用（ecall），触发系统调用
        "sd a0, %0"     // 将返回值存入 ret 变量
        : "=m" (ret)    // 输出操作数，将 a0 寄存器的值存到 ret
        : "m"(num), "m"(a[0]), "m"(a[1]), "m"(a[2]), "m"(a[3]), "m"(a[4]) // 输入操作数，将参数传递到寄存器
        : "memory"      // 告诉编译器函数内对内存的读写操作
    );
    // num 存储到 a0 寄存器，a[0] 存储到 a1 寄存器，依此类推
    // ecall 的返回值存储到 ret 中
    return ret;
}


int
sys_exit(int64_t error_code) {
    return syscall(SYS_exit, error_code);
}

int
sys_fork(void) {
    return syscall(SYS_fork);
}

int
sys_wait(int64_t pid, int *store) {
    return syscall(SYS_wait, pid, store);
}

int
sys_yield(void) {
    return syscall(SYS_yield);
}

int
sys_kill(int64_t pid) {
    return syscall(SYS_kill, pid);
}

int
sys_getpid(void) {
    return syscall(SYS_getpid);
}

int
sys_putc(int64_t c) {
    return syscall(SYS_putc, c);
}

int
sys_pgdir(void) {
    return syscall(SYS_pgdir);
}

