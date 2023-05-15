#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include "aes.h"
#include <x86intrin.h>


#define AESSIZE 16

typedef uint8_t aes_t[AESSIZE];


void tobinary(char *data, aes_t aes) {
  assert(strlen(data)==AESSIZE*2);
  unsigned int x;
  for (int i = 0; i < AESSIZE; i++) {
    sscanf(data+i*2, "%2x", &x);
    aes[i] = x;
  }
}

char *toString(aes_t aes) {
  char buf[AESSIZE * 2 + 1];
  for (int i = 0; i < AESSIZE; i++)
    sprintf(buf + i*2, "%02x", aes[i]);
  return strdup(buf);
}

void randaes(aes_t aes) {
  for (int i = 0; i < AESSIZE; i++)
    aes[i] = rand() & 0xff;
}


/*
int main(int ac, char **av) {
  aes_t key, input, output;
  tobinary("00000000000000000000000000000000", key);
  AES_KEY aeskey;
  private_AES_set_encrypt_key(key, 128, &aeskey);

  tobinary("00000000000000000000000000000000", input);
  AES_encrypt(input, output, &aeskey);
  char *x = toString(output);
  printf("%s\n", x);
  free(x);
}
*/

void generate_random(unsigned char *out, size_t length) {
    for (size_t i = 0; i < length; i++)
        out[i] = (unsigned char)rand();
}

void warmup() {
    unsigned int dummy;
    uint64_t start_wait = __rdtscp(&dummy);
    uint64_t wait;
    do {
        wait = __rdtscp(&dummy);
    } while ((wait - start_wait) < 2000000000);
}

typedef uint32_t u32;
extern const u32 Te0[256];

int main() {
    warmup();

    uint64_t measurements_sum[256 >> 4];
    uint64_t measurements_count[256 >> 4];
    memset(measurements_sum, 0, sizeof(measurements_sum));
    memset(measurements_count, 0, sizeof(measurements_count));

    unsigned int dummy;
    srand(time(NULL));
    unsigned char plaintext[16];
    unsigned char ciphertext[16];

    aes_t key;
    tobinary("CCCCCCC15CCC11011101110100011111", key);
    AES_KEY aeskey;

    aeskey.rounds = 14;
    private_AES_set_encrypt_key(key, 128, &aeskey);

    printf("%u\n", (unsigned)( ((unsigned char *)(&key))[0] ) >> 4);

    for (int i = 0; i < 1000000; i++) {
        generate_random(plaintext, 16);

        AES_encrypt(plaintext, ciphertext, &aeskey);
        _mm_clflush(&Te0[0]);
        _mm_mfence();

        uint64_t start = __rdtscp(&dummy);
        _mm_lfence();
        AES_encrypt(plaintext, ciphertext, &aeskey);
        uint64_t diff = __rdtscp(&dummy) - start;

        measurements_sum[plaintext[0] >> 4] += diff;
        measurements_count[plaintext[0] >> 4]++;
    }

    int max_i = -1;
    double max_avg = -1;

    for (int i = 0; i < (256 >> 4); i++) {
        if (measurements_count[i] == 0) {
            printf("%d: undefined\n", i);
        } else {
            double avg = (float)measurements_sum[i] / measurements_count[i];
            if (avg > max_avg) {
                max_avg = avg;
                max_i = i;
            }
            printf("%d: %lu (%lu)\n", i, (uint64_t)avg, measurements_count[i]);
        }
    }

    printf("max: %d (%f)\n", max_i, max_avg);

    return 0;
}
