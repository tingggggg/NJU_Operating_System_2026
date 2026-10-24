#include <stdarg.h>
#include <stdio.h>

// Read n integer arguments after n and print them.
void foo(int n, ...) {
    va_list ap;
    va_start(ap, n);

    for (int i = 0; i < n; i++) {
        int volatile v = va_arg(ap, int);
    }

    va_end(ap);
}

int main() {
    // 1 argument after n
    foo(1, 11);

    // 5 arguments after n
    foo(5, 1, 2, 3, 4, 5);

    // 10 arguments after n
    foo(10, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100);

    return 0;
}
