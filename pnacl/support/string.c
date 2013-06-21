/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Implementation of C library functions that may be required by lowering
 * intrinsics that are part of the PNaCl stable bitcode ABI.
 */

/* Nonzero if X is not aligned "long" boundary. */
#define UNALIGNED(X) (((long) X & (sizeof(long) - 1)))

/* How many bytes are copied each iteration of the 4X unrolled loop.  */
#define BIGBLOCKSIZE    (sizeof(long) << 2)

/* How many bytes are copied each iteration of the word copy loop.  */
#define LITTLEBLOCKSIZE (sizeof(long))

/* Threshhold for punting to the byte copier.  */
#define TOO_SMALL(LEN)  ((LEN) < BIGBLOCKSIZE)


/* The memset() function fills the first n bytes of the memory area pointed to
 * by s_void with the constant byte c.
 */
void *memset(void *s_void, int c, unsigned n) {
  char *s = (char*) s_void;
  int i;
  unsigned long buffer;
  unsigned long *aligned_addr;
  /* To avoid sign extension, copy C to an unsigned variable. */
  unsigned int d = c & 0xff;

  while (UNALIGNED(s)) {
    if (n--)
      *s++ = (char) c;
    else
      return s_void;
  }

  if (!TOO_SMALL(n)) {
    /* If we get this far, we know that n is large and s is word-aligned. */
    aligned_addr = (unsigned long*) s;

    /* Store D into each char sized location in BUFFER so that
     * we can set large blocks quickly.
     */
    buffer = (d << 8) | d;
    buffer |= (buffer << 16);
    for (i = 32; i < LITTLEBLOCKSIZE * 8; i <<= 1)
      buffer = (buffer << i) | buffer;

    /* Unroll the loop. */
    while (n >= LITTLEBLOCKSIZE * 4) {
      *aligned_addr++ = buffer;
      *aligned_addr++ = buffer;
      *aligned_addr++ = buffer;
      *aligned_addr++ = buffer;
      n -= 4 * LITTLEBLOCKSIZE;
    }

    while (n >= LITTLEBLOCKSIZE) {
      *aligned_addr++ = buffer;
      n -= LITTLEBLOCKSIZE;
    }

    /* Pick up the remainder with a bytewise loop. */
    s = (char*) aligned_addr;
  }

  while (n--)
    *s++ = (char) c;

  return s_void;
}

/* The memcpy() function copies n bytes from memory area src_void to memory
 * area dst_void. The memory areas must not overlap.
 */
void *memcpy(void *dst_void, const void *src_void, unsigned n) {
  char *dst = dst_void;
  char *src = (char*) src_void;
  long *aligned_dst;
  long *aligned_src;

  /* If the size is small, or either SRC or DST is unaligned,
   * then punt into the byte copy loop. This should be rare.
   */
  if (!TOO_SMALL(n) && !(UNALIGNED(src) || UNALIGNED(dst))) {
    aligned_dst = (long*) dst;
    aligned_src = (long*) src;

    /* Copy 4X long words at a time if possible. */
    while (n >= BIGBLOCKSIZE) {
      *aligned_dst++ = *aligned_src++;
      *aligned_dst++ = *aligned_src++;
      *aligned_dst++ = *aligned_src++;
      *aligned_dst++ = *aligned_src++;
      n -= BIGBLOCKSIZE;
    }

    /* Copy one long word at a time if possible. */
    while (n >= LITTLEBLOCKSIZE) {
      *aligned_dst++ = *aligned_src++;
      n -= LITTLEBLOCKSIZE;
    }

    /* Pick up any residual with a byte copier. */
    dst = (char*) aligned_dst;
    src = (char*) aligned_src;
  }

  while (n--) {
    *dst++ = *src++;
  }

  return dst_void;
}

/* The memmove() function copies n bytes from memory area src_void to memory
 * area dst_void. The memory areas may overlap.
 */
void *memmove(void *dst_void, const void *src_void, unsigned n) {
  char *dst = dst_void;
  char *src = (char*) src_void;
  long *aligned_dst;
  long *aligned_src;

  if (src < dst && dst < src + n) {
    /* Destructive overlap...have to copy backwards */
    src += n;
    dst += n;
    while (n--) {
      *--dst = *--src;
    }
  } else {
    /* Use optimizing algorithm for a non-destructive copy to closely
     * match memcpy. If the size is small or either SRC or DST is unaligned,
     * then punt into the byte copy loop. This should be rare.
     */
    if (!TOO_SMALL(n) && !(UNALIGNED(src) || UNALIGNED(dst))) {
      aligned_dst = (long*) dst;
      aligned_src = (long*) src;

      /* Copy 4X long words at a time if possible. */
      while (n >= BIGBLOCKSIZE) {
        *aligned_dst++ = *aligned_src++;
        *aligned_dst++ = *aligned_src++;
        *aligned_dst++ = *aligned_src++;
        *aligned_dst++ = *aligned_src++;
        n -= BIGBLOCKSIZE;
      }

      /* Copy one long word at a time if possible. */
      while (n >= LITTLEBLOCKSIZE) {
        *aligned_dst++ = *aligned_src++;
        n -= LITTLEBLOCKSIZE;
      }

      /* Pick up any residual with a byte copier. */
      dst = (char*) aligned_dst;
      src = (char*) aligned_src;
    }

    while (n--) {
      *dst++ = *src++;
    }
  }

  return dst_void;
}

