#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

int main(void) {
    struct timeval t1, t2;

    if (gettimeofday(&t1, NULL) != 0) {
        perror("gettimeofday(t1)");
        return 1;
    }

    usleep(200000);  // 200 ms

    if (gettimeofday(&t2, NULL) != 0) {
        perror("gettimeofday(t2)");
        return 1;
    }

    long sec = t2.tv_sec - t1.tv_sec;
    long usec = t2.tv_usec - t1.tv_usec;
    long elapsed_us = sec * 1000000L + usec;

    printf("t1: %ld.%06ld\n", (long)t1.tv_sec, (long)t1.tv_usec);
    printf("t2: %ld.%06ld\n", (long)t2.tv_sec, (long)t2.tv_usec);
    printf("elapsed: %ld us (%.3f ms)\n", elapsed_us, elapsed_us / 1000.0);

    return 0;
}
