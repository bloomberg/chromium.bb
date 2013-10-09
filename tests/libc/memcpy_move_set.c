/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Simple test to verify that memcpy, memmove and memset are found and
 * work properly.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Create buffer as an array of long, to ensure word-size alignment. The
 * actual code accesses it via a char*.
 */
#undef WORDSIZE
#define NWORDS    32
#define WORDSIZE  sizeof(long)
#define LENGTH    NWORDS * WORDSIZE
volatile unsigned length = LENGTH;
volatile long buf[NWORDS] = {0};


/*
 * Reset global buf to the sequence of bytes: 0, 1, 2 ... LENGTH - 1
 */
void reset_buf(void) {
  unsigned char *bufptr = (unsigned char*) buf;
  unsigned i;
  for (i = 0; i < length; ++i)
    bufptr[i] = i;
}

/*
 * Each function we're testing has a "checked" version that runs it and makes
 * sure the destination pointer is returned correctly.
 */
void checked_memcpy(void *dst, void *src, unsigned n) {
  void *ret = memcpy(dst, src, n);
  if (ret != dst) {
    printf("Wrong memcpy return value: %p != %p\n", ret, dst);
    exit(1);
  }
}

void checked_memmove(void *dst, void *src, unsigned n) {
  void *ret = memmove(dst, src, n);
  if (ret != dst) {
    printf("Wrong memmove return value: %p != %p\n", ret, dst);
    exit(1);
  }
}

void checked_memset(void *s, int c, unsigned n) {
  void *ret = memset(s, c, n);
  if (ret != s) {
    printf("Wrong memset return value: %p != %p\n", ret, s);
    exit(1);
  }
}

int main(void) {
  /* arrptr is an aligned pointer to the buffer. */
  unsigned char *arrptr = (unsigned char*) buf, *src, *dst;
  if ((long) arrptr & (WORDSIZE - 1)) {
    puts("Internal error: unaligned buf\n");
    return 1;
  }
  reset_buf();

  /*
   * Test 1: memcpy small chunk, from aligned to aligned address.
   * "small chunk" is anything smaller than UNROLLBLOCKSIZE in our
   * implementation of these functions.
   */
  reset_buf();
  src = arrptr;
  dst = arrptr + 16 * WORDSIZE;
  checked_memcpy(dst, src, 6);
  printf("1: %u\n", (unsigned)dst[4]);    /* expect 4 */

  /* Test 2: memcpy small chunk, from aligned to unaligned address */
  reset_buf();
  src = arrptr;
  dst = arrptr + 16 * WORDSIZE + 1;
  checked_memcpy(dst, src, 6);
  printf("2: %u\n", (unsigned)dst[4]);    /* expect 4 */

  /* Test 3: memcpy small chunk, from unaligned to aligned address */
  reset_buf();
  src = arrptr + 1;
  dst = arrptr + 16 * WORDSIZE;
  checked_memcpy(dst, src, 6);
  printf("3: %u\n", (unsigned)dst[4]);    /* expect 5 */

  /* Test 4: memcpy small chunk, from unaligned to unaligned address */
  reset_buf();
  src = arrptr + 3;
  dst = arrptr + 16 * WORDSIZE + 3;
  checked_memcpy(dst, src, 6);
  printf("4: %u\n", (unsigned)dst[4]);    /* expect 7 */

  /* Test 5: memcpy large chunk, from aligned to aligned address */
  reset_buf();
  src = arrptr;
  dst = arrptr + 16 * WORDSIZE;
  checked_memcpy(dst, src, 8 * WORDSIZE);
  printf("5: %u\n", (unsigned)dst[30]);    /* expect 30 */

  /* Test 6: memcpy large chunk, from aligned to unaligned address */
  reset_buf();
  src = arrptr;
  dst = arrptr + 16 * WORDSIZE + 1;
  checked_memcpy(dst, src, 8 * WORDSIZE);
  printf("6: %u\n", (unsigned)dst[30]);    /* expect 30 */

  /* Test 7: memcpy large chunk, from unaligned to aligned address */
  reset_buf();
  src = arrptr + 1;
  dst = arrptr + 16 * WORDSIZE;
  checked_memcpy(dst, src, 8 * WORDSIZE);
  printf("7: %u\n", (unsigned)dst[30]);    /* expect 31 */

  /* Test 8: memcpy large chunk, from unaligned to unaligned address */
  reset_buf();
  src = arrptr + 3;
  dst = arrptr + 16 * WORDSIZE + 3;
  checked_memcpy(dst, src, 8 * WORDSIZE);
  printf("8: %u\n", (unsigned)dst[30]);    /* expect 33 */

  /* Test 9: memcpy large chunk, near edges/overlap */
  reset_buf();
  src = arrptr;
  dst = arrptr + length / 2;
  checked_memcpy(dst, src, length / 2);
  printf("9: %u\n", (unsigned)dst[10]);    /* expect 10 */

  /* Test 100: memset small chunk, aligned address */
  reset_buf();
  checked_memset(arrptr, 99, 5);
  printf("100: %u\n", (unsigned)arrptr[4]);   /* expect 99 */

  /* Test 101: memset small chunk, unaligned address */
  reset_buf();
  checked_memset(arrptr + 3, 99, 5);
  printf("101a: %u\n", (unsigned)arrptr[7]);   /* expect 99 */
  printf("101b: %u\n", (unsigned)arrptr[2]);   /* expect 2 */
  printf("101c: %u\n", (unsigned)arrptr[8]);   /* expect 8 */

  /* Test 102: memset large chunk, aligned address */
  reset_buf();
  checked_memset(arrptr, 99, 8 * WORDSIZE);
  printf("102: %u\n", (unsigned)arrptr[31]);   /* expect 99 */

  /* Test 103: memset large chunk, unaligned address */
  reset_buf();
  checked_memset(arrptr + 3, 99, 8 * WORDSIZE);
  printf("103: %u\n", (unsigned)arrptr[34]);   /* expect 99 */

  /* Test 104: edge */
  reset_buf();
  checked_memset(arrptr, 99, length);
  printf("104: %u\n", (unsigned)arrptr[LENGTH - 1]);   /* expect 99 */

  /*
   * The non-overlapping logic of memmove is pretty much the same as memcpy.
   * Do a sanity check and then test overlapping addresses.
   */

  /* Test 201: memmove large chunk, from aligned to aligned address */
  reset_buf();
  src = arrptr;
  dst = arrptr + 16 * WORDSIZE;
  checked_memmove(dst, src, 8 * WORDSIZE);
  printf("201: %u\n", (unsigned)dst[31]);    /* expect 31 */

  /* Test 202: memmove small chunk in overlapping addresses */
  reset_buf();
  src = arrptr + 4;
  dst = arrptr;
  checked_memmove(dst, src, 8);
  printf("202: %u\n", (unsigned)dst[7]);     /* expect 11 */

  /* Test 203: memmove large chunk in overlapping addresses */
  reset_buf();
  src = arrptr + 1;
  dst = arrptr;
  checked_memmove(dst, src, 16 * WORDSIZE);
  printf("203: %u\n", (unsigned)dst[63]);    /* expect 64 */

  /* Test 204: memmove at edge */
  reset_buf();
  src = arrptr + 1;
  dst = arrptr;
  checked_memmove(dst, src, LENGTH - 1);
  printf("204: %u\n", (unsigned)dst[LENGTH - 2]);    /* expect LENGTH-1 */

  return 0;
}

