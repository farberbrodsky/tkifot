#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <x86intrin.h>
#include <sys/mman.h>

#define WAYS (12)
#define SET_BITS (6)
#define CACHE_LINE_BITS (6)
#define PAGE_BITS (12)
#define BUFFER_SIZE (WAYS << (SET_BITS + CACHE_LINE_BITS))

volatile char *buffer;
volatile char *victim_buffer;

void victim(size_t set_index) {
    char c = victim_buffer[set_index << SET_BITS];
    asm volatile(" " : : "r"(c) : "memory");
}

void warmup() {
    unsigned int dummy;
    uint64_t start_wait = __rdtscp(&dummy);
    uint64_t wait;
    do {
        wait = __rdtscp(&dummy);
    } while ((wait - start_wait) < 2000000000);
}

void prime_set(size_t set_index) {
    volatile char *set_base = &buffer[set_index << CACHE_LINE_BITS];

    for (unsigned i = 0; i < WAYS; i++) {
        char c = set_base[i << (SET_BITS + CACHE_LINE_BITS)];
        asm volatile(" " : : "r"(c) : "memory");
    }
}

uint64_t prime_probe_set(size_t set_index) {
    unsigned int dummy;
    volatile char *set_base = &buffer[set_index << CACHE_LINE_BITS];

    uint64_t start = __rdtscp(&dummy);
    asm volatile("lfence" : : : "memory");

    for (unsigned i = 0; i < WAYS; i++) {
        char c = set_base[i << (SET_BITS + CACHE_LINE_BITS)];
        asm volatile(" " : : "r"(c) : "memory");
    }

    return __rdtscp(&dummy) - start;
}

int main() {
    buffer = mmap(0, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (buffer == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    victim_buffer = mmap(0, 1 << (SET_BITS + CACHE_LINE_BITS), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (victim_buffer == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    // allocate buffer
    for (uint i = 0; i < (BUFFER_SIZE >> PAGE_BITS); i++) {
        buffer[i << PAGE_BITS] = 1;
    }
    victim_buffer[0] = 1;

    warmup();
    uint64_t sum_with = 0;
    uint64_t count_with = 0;
    uint64_t sum_without = 0;
    uint64_t count_without = 0;
    size_t my_set = 60;

    for (int i = 0; i < 100000; i++) {
        prime_set(my_set);
        uint64_t probe_result = prime_probe_set(my_set);

        if (probe_result < 200) {
            sum_without += probe_result;
            count_without++;
        }

        prime_set(my_set);
        victim(my_set);
        probe_result = prime_probe_set(my_set);

        if (probe_result < 200) {
            sum_with += probe_result;
            count_with++;
        }
    }

    if (count_with < 99000 || count_without < 99000) {
        printf("ERROR: too many lost samples\n");
        return 1;
    }

    double avg_with = (double)sum_with / (double)count_with;
    double avg_without = (double)sum_without / (double)count_without;

    printf("average with victim: %f, average without: %f, diff: %f\n", avg_with, avg_without, avg_with - avg_without);
    return 0;
}
