#include <stdio.h>
#include <setjmp.h>

jmp_buf buf;

void second(int x) {
    printf("In second(), x = %d\n", x);
    longjmp(buf, x);
}

void first() {
    printf("In first()\n");
    second(42);
    printf("This line should not be printed (after longjmp)\n");
}

int main() {
    int retval;

    retval = setjmp(buf);

    if (retval == 0) {
        printf("setjmp returns 0; calling first()\n");
        first();
    } else {
        printf("Returned to main via longjmp, retval = %d\n", retval);
    }

    return 0;
}
