int main();

// Should be placed at the very beginning of the program.
void _start() {
    main();
}

#include <stdarg.h>

static inline void sys_putchar(char ch) {
    register int a0 asm("a0") = ch;
    register int a7 asm("a7") = 42;
    asm volatile ("ecall" : "+r"(a0) : "r"(a7));
}

static inline void print_str(const char *s) {
    char c = *s;
    while (c != '\0') {
        sys_putchar(c);
        s = s + 1;
        c = *s;
    }
}

static inline void print_num(int n) {
    int q, r, digit;
    if (n < 0) {
        sys_putchar('-');
        n = -n;
    }
    q = n / 10;
    if (q != 0) {
        print_num(q);
    }
    r = n % 10;
    digit = '0' + r;
    sys_putchar(digit);
}

static inline void myprintf(const char *fmt, ...) {
    va_list ap;
    char c;
    const char *s;
    int d;

    va_start(ap, fmt);

    while (1) {
        c = *fmt;
        if (c == '\0') {
            break;
        }
        if (c != '%') {
            sys_putchar(c);
            fmt = fmt + 1;
            continue;
        }
        fmt = fmt + 1;
        c = *fmt;
        if (c == '\0') {
            sys_putchar('%');
            break;
        }
        if (c == 's') {
            s = va_arg(ap, const char *);
            print_str(s);
            fmt = fmt + 1;
            continue;
        }
        if (c == 'd') {
            d = va_arg(ap, int);
            print_num(d);
            fmt = fmt + 1;
            continue;
        }
        sys_putchar('%');
        sys_putchar(c);
        fmt = fmt + 1;
    }

    va_end(ap);
}
