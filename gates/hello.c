#include <stdio.h>
#include <x86intrin.h>
#include <sys/mman.h>
#include <stdint.h>
#include <assert.h>

#define SET_COUNT 64
#define MEASUREMENTS 100
#define noin __attribute__ ((noinline))

void warmup() {
    unsigned int dummy;
    uint64_t start_wait = __rdtscp(&dummy);
    uint64_t wait;
    do {
        wait = __rdtscp(&dummy);
    } while ((wait - start_wait) < 2000000000);
}

static uint64_t timestamp() {
    unsigned int dummy;
    return __rdtscp(&dummy);
}

static uint64_t blocking_timestamp() {
    uint64_t result = timestamp();
    _mm_lfence();
    return result;
}

static double measurements[MEASUREMENTS];
int measurement_compare(const void *x, const void *y) {
    const double *m = (const double *)x;
    const double *n = (const double *)y;

    if (m < n)
        return -1;
    else if (m > n)
        return 1;
    else
        return 0;
}

// sorts measurements based on this set and gets the average without the bottom 10% or top 10%
static double measurements_cutout_average() {
    qsort(measurements, MEASUREMENTS, sizeof(double), measurement_compare);
    double sum = 0;
    double count = 0;

    for (int m = MEASUREMENTS / 10; m < ((MEASUREMENTS * 9) / 10); m++) {
        sum += measurements[m];
        count++;
    }

    assert(count != 0);
    return sum / count;
}

static volatile char *a;
static volatile char *b;

noin void gate1(volatile char *in) {
    if (!*in) {
        return;
    }

    // waste time
    asm volatile("mull %%eax" : : : "%eax", "%edx");
    asm volatile("mull %%eax" : : : "%eax", "%edx");
    asm volatile("mull %%eax" : : : "%eax", "%edx");
    asm volatile("mull %%eax" : : : "%eax", "%edx");
    asm volatile("mull %%eax" : : : "%eax", "%edx");
    asm volatile("mull %%eax" : : : "%eax", "%edx");

    // finally, load a
    *a = *in;
}

void train_gate1_taken() {
    for (int i = 0; i < 1000; i++) {
        gate1("x");
    }
}

void measure_with_flush() {
    uint64_t with_flush = 0;
    for (int m = 0; m < MEASUREMENTS; m++) {
        train_gate1_taken();
        _mm_clflush((void *)b);
        gate1(b);

        uint64_t t1 = blocking_timestamp();

        // time loading a
        asm volatile("" : : "r"(*a) : "memory");

        measurements[m] = timestamp() - t1;
    }
    printf("when b not in cache: time loading a is %f\n", measurements_cutout_average());
}

void measure_without_flush() {
    uint64_t without_flush = 0;
    for (int m = 0; m < MEASUREMENTS; m++) {
        train_gate1_taken();
        asm volatile("" : : "r"(*b) : "memory");
        gate1(b);

        uint64_t t1 = blocking_timestamp();

        // time loading a
        asm volatile("" : : "r"(*a) : "memory");

        measurements[m] = timestamp() - t1;
    }
    printf("when b in cache: time loading a is %f\n", measurements_cutout_average());

}

int main() {
    warmup();
    a = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    b = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    assert(a != MAP_FAILED);
    assert(b != MAP_FAILED);
    b[0] = 0;

    measure_without_flush();
    measure_with_flush();
    measure_without_flush();
    measure_with_flush();

    return 0;
}
