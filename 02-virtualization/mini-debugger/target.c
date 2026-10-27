#include <stdio.h>

int square(int x) { return x * x; }

int main() {
  for (int i = 1; i <= 3; i++)
    printf("square(%d) = %d\n", i, square(i));
  return 0;
}
