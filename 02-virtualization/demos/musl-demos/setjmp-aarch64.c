#include <setjmp.h>
#include <stdio.h>

static jmp_buf checkpoint;

static inline void set_gprs_to_0x114(void) {
    asm volatile(
        "mov x0,  #0x114\n\t"
        "mov x1,  #0x114\n\t"
        "mov x2,  #0x114\n\t"
        "mov x3,  #0x114\n\t"
        "mov x4,  #0x114\n\t"
        "mov x5,  #0x114\n\t"
        "mov x6,  #0x114\n\t"
        "mov x7,  #0x114\n\t"
        "mov x8,  #0x114\n\t"
        "mov x9,  #0x114\n\t"
        "mov x10, #0x114\n\t"
        "mov x11, #0x114\n\t"
        "mov x12, #0x114\n\t"
        "mov x13, #0x114\n\t"
        "mov x14, #0x114\n\t"
        "mov x15, #0x114\n\t"
        "mov x16, #0x114\n\t"
        "mov x17, #0x114\n\t"
        "mov x18, #0x114\n\t"
        "mov x19, #0x114\n\t"
        "mov x20, #0x114\n\t"
        "mov x21, #0x114\n\t"
        "mov x22, #0x114\n\t"
        "mov x23, #0x114\n\t"
        "mov x24, #0x114\n\t"
        "mov x25, #0x114\n\t"
        "mov x26, #0x114\n\t"
        "mov x27, #0x114\n\t"
        "mov x28, #0x114\n\t"
        :
        :
        : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
          "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15",
          "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23",
          "x24", "x25", "x26", "x27", "x28", "memory");
}

static inline void set_gprs_to_0x514(void) {
    asm volatile(
        "mov x0,  #0x514\n\t"
        "mov x1,  #0x514\n\t"
        "mov x2,  #0x514\n\t"
        "mov x3,  #0x514\n\t"
        "mov x4,  #0x514\n\t"
        "mov x5,  #0x514\n\t"
        "mov x6,  #0x514\n\t"
        "mov x7,  #0x514\n\t"
        "mov x8,  #0x514\n\t"
        "mov x9,  #0x514\n\t"
        "mov x10, #0x514\n\t"
        "mov x11, #0x514\n\t"
        "mov x12, #0x514\n\t"
        "mov x13, #0x514\n\t"
        "mov x14, #0x514\n\t"
        "mov x15, #0x514\n\t"
        "mov x16, #0x514\n\t"
        "mov x17, #0x514\n\t"
        "mov x18, #0x514\n\t"
        "mov x19, #0x514\n\t"
        "mov x20, #0x514\n\t"
        "mov x21, #0x514\n\t"
        "mov x22, #0x514\n\t"
        "mov x23, #0x514\n\t"
        "mov x24, #0x514\n\t"
        "mov x25, #0x514\n\t"
        "mov x26, #0x514\n\t"
        "mov x27, #0x514\n\t"
        "mov x28, #0x514\n\t"
        :
        :
        : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
          "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15",
          "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23",
          "x24", "x25", "x26", "x27", "x28", "memory");
}

int main(void) {
    set_gprs_to_0x114();

    int jmp_rc = setjmp(checkpoint);
    if (jmp_rc == 0) {
        set_gprs_to_0x514();
        longjmp(checkpoint, 1);
    }

    puts("Returned via longjmp; inspect registers in debugger.");
    return 0;
}
