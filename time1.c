#include <stdio.h>
#include <stdint.h>
#include <x86intrin.h>

int main() {
    unsigned int dummy;
    uint64_t sum = 0ULL;
    uint64_t start;

    uint64_t start_wait = __rdtscp(&dummy);
    uint64_t wait;
    do {
        wait = __rdtscp(&dummy);
    } while ((wait - start_wait) < 1000000000);

    int n = 0;
    for (int i = 0; i < 10000; i++) {
        start = __rdtscp(&dummy);
        n++;
        asm volatile(" " : : "r" (n): "memory");
        sum += __rdtscp(&dummy) - start;
    }

    printf("%lu.%04lu (%d)\n", sum / 10000, sum % 10000, n);

    sum = 0;

    volatile char *buf = malloc(0x1337000);
    volatile char *ptr = &buf[0x222200];

    for (int i = 0; i < 10000; i++) {
        uint32_t x1, x2, y1, y2;
        char c = *ptr;
        asm volatile(" " : : "r"(c) : "memory");

        // this part is measured
        asm volatile("rdtscp" : "=a"(x1), "=d"(x2) : : "memory");
        asm volatile("lfence" : : : "memory");

        c = *ptr;
        asm volatile(" " : : "r"(c) : "memory");
        asm volatile("rdtscp" : "=a"(y1), "=d"(y2) : : "memory");

        sum += (((uint64_t)y1 << 32ULL) | y2) - (((uint64_t)x1 << 32ULL) | x2);
    }

    printf("in cache: %lu.%04lu\n", sum / 10000, sum % 10000);

    for (int i = 0; i < 10000; i++) {
        _mm_clflush((void *)ptr);

        // this part is measured
        start = __rdtscp(&dummy);
        asm volatile("lfence" : : : "memory");

        char c = *ptr;
        asm volatile(" " : : "r"(c) : "memory");

        sum += __rdtscp(&dummy) - start;
    }

    printf("flushed: %lu.%04lu\n", sum / 10000, sum % 10000);
}
