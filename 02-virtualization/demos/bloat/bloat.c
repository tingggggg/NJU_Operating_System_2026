__attribute__((noinline, used))
void bloat() {
    asm volatile(
        ".rept 10485760 \n\t"
        "nop\n\t"
        ".endr\n\t"
    );
}
