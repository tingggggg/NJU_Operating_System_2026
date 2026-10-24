#include <stdio.h>
#include <inttypes.h>

int main() {
    char buf[64];

    // Demo PRI series of macros for portable printf of fixed-width types.
    int32_t si = -12345;
    uint64_t ul = 1234567890123456789ULL;
    intptr_t intptr_x = 0xfeedface;
    uintptr_t uintptr_y = 0xdeadbeef;

    printf("PRI series demo:\n");
    printf("int32_t: %" PRId32 "\n", si);
    printf("uint64_t: %" PRIu64 "\n", ul);
    printf("intptr_t: %" PRIxPTR "\n", intptr_x);
    printf("uintptr_t: %" PRIuPTR "\n", uintptr_y);

    // Also show snprintf + PRI macros
    snprintf(buf, sizeof(buf), "snprintf: %" PRId32 ", 0x%" PRIxPTR, si, intptr_x);
    puts(buf);
}
