/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>


/*
 * The integrated runtime (IRT) needs to have a malloc() heap that is
 * isolated from that of the user executable.
 *
 * The IRT uses newlib's malloc(), which uses sbrk() and is not
 * sophisticated enough to fall back to mmap() if sbrk() fails (unlike
 * glibc's malloc() implementation).  Therefore we provide a very
 * simple-minded sbrk() implementation here.
 *
 * This function overrides the sbrk() in libnacl (src/untrusted/nacl/sbrk.c).
 */

/* Allocate an ungrowable 16MB region. */
const static size_t arena_size = 16 << 20;

static char *available = NULL;
static char *arena_end;

/*
 * Note that this is not thread-safe.  We expect the malloc()
 * implementation that uses sbrk() to do its own locking.
 */
void *sbrk(intptr_t increment) {
  void *result;

  if (available == NULL) {
    available = mmap(NULL, arena_size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (available == MAP_FAILED) {
      return MAP_FAILED;
    }
    arena_end = available + arena_size;
  }
  if (increment <= 0) {
    /* For simplicity we do not handle freeing memory. */
    return available;
  }
  if (available + increment > arena_end) {
    /*
     * We do not expect the malloc() implementation to cope with
     * sbrk() returning discontiguous memory regions, so we do not try
     * mmap()ing another arena.
     */
    errno = ENOMEM;
    return MAP_FAILED;
  }
  result = available;
  available += increment;
  return result;
}
