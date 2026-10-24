#include <math.h>
#include <stdio.h>

int main() {
    float a = NAN;

    volatile int x = (a > a);
    volatile int y = (a < a);
    volatile int z = (a == a);

    printf("x = %d, y = %d, z = %d\n", x, y, z);

    int result = !(x || y || z);
    printf("Demo: !(a > a || a < a || a == a) = %d\n", result); // will print 1
    return 0;
}
