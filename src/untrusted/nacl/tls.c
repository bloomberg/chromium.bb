/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * Native Client support for thread local storage
 */

#include <sys/unistd.h>
#include "native_client/src/untrusted/nacl/tls.h"

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

#define STACK_ALIGNMENT 32
#if NACL_ARCH(NACL_TARGET_ARCH) == NACL_arm
#define TLS_ALIGNMENT (1 << 12)
#else
#define TLS_ALIGNMENT 32
#endif

#define THREAD_STACK_SIZE (512 * 1024) /* 512K */


/*
 * These stubs provide setup for thread local storage when libpthread is not
 * being used.  Since they are declared as weak symbols, they are overridden
 * if libpthread is used.
 */

/* @IGNORE_LINES_FOR_CODE_HYGIENE[2] */
extern int __pthread_initialize() __attribute__ ((weak));
extern int __pthread_shutdown() __attribute__ ((weak));

/*
 * Symbols defined by the linker, we copy those sections using them
 * as templates
 */

/* @IGNORE_LINES_FOR_CODE_HYGIENE[3] */
extern char __tls_template_start;
extern char __tls_template_tdata_end;
extern char __tls_template_end;


/* We can find TLS sections using symbols created by the linker */
#define TLS_TDATA_START (&__tls_template_start)
#define TLS_TDATA_SIZE (&__tls_template_tdata_end - TLS_TDATA_START)
#define TLS_TBSS_SIZE (&__tls_template_end - &__tls_template_tdata_end)


/* TODO(gregoryd) - consider using real memcpy and memset
 *                  (we do not want to be forced to link in  (new)libc)
 */

static void my_memcpy(void* dest, const void* src, size_t size) {
  /* TODO(gregoryd): optimize to use DWORDs instead of bytes */
  size_t i;

  for (i = 0; i < size; ++i) {
    *((char*)dest + i) = *((char*)src + i);
  }
}


static void my_memset(void* s, int c, size_t size) {
  size_t i;
  for (i = 0; i < size; ++i) {
    *((char*)s + i) = (char) c;
  }
}


/* NOTE: assumes power of 2 alignment */
static uint32_t Align(uint32_t offset, uint32_t alignment) {
  return (offset + alignment - 1) & ~(alignment - 1);
}


static char *__nacl_tls_align(void* p) {
  return (char *) Align((uint32_t) p, TLS_ALIGNMENT);
}


size_t __nacl_tls_combined_size(size_t tdb_size, int add_fluff) {
  int fluff = 0;
  if (add_fluff) fluff = TLS_ALIGNMENT - 1;
#if NACL_ARCH(NACL_TARGET_ARCH) == NACL_arm
  /* NOTE: on arm the tdb is already part of tls area */
  tdb_size = 0;
#endif
  return fluff + TLS_TDATA_SIZE + TLS_TBSS_SIZE + tdb_size;
}


size_t __nacl_thread_stack_size(int add_fluff) {
  int fluff = 0;
  if (add_fluff) fluff = STACK_ALIGNMENT - 1;
  return fluff + THREAD_STACK_SIZE;
}


char *__nacl_thread_stack_align(void* p) {
  return (char *) Align((uint32_t) p, STACK_ALIGNMENT);
}

/* the organization of the tls block (aka combined tls area) in nacl is
 *
 * for x86:
 *  [fluff]     (TLS_ALIGNEMNT waste)
 *  TLS_DATA
 *  TLS_BSS
 *  TDB         (This must be align too, as it becomes a segment
 *
 * for ARM:
 *  [fluff]     (2^12 alignment waste)
 *  TDB
 *  TLS_DATA
 *  TLS_BSS
 *
 *
 * The size of TLS_DATA/TLS_BSS are multiples of 32 (enforced
 * by the linker) which coincidences with TLS_ALIGNEMNT for x86.
 * On x86 the tls area is accessed as negative offsets from
 * the end of the area (= beginning of TDB) on arm we use positive offsets
 * from the beginning of TLS_DATA. For both platforms the first word of the
 * TDB contains this "base" but it is only used for x86.
 */

static void* __nacl_tls_data_start(void* p) {
  return __nacl_tls_align(p);
}


static void* __nacl_tls_bss_start(void* p) {
  return __nacl_tls_align(p) + TLS_TDATA_SIZE;
}

/* on ARM the tdb is at the beginning of the tls are (c.f. crti_arm.S)
 * on x86 the tdb follows after the tls area
 * TODO(gregoryd): consider moving this into crtn_x86.S
 */

void* __nacl_tls_tdb_start(void* p) {
#if NACL_ARCH(NACL_TARGET_ARCH) == NACL_arm
  return __nacl_tls_data_start(p);
#else
  return __nacl_tls_align(p) + TLS_TDATA_SIZE + TLS_TBSS_SIZE;
#endif
}


/* this assumes that combined_area is already aligned */
void __nacl_tls_data_bss_initialize_from_template(void* combined_area) {
  /* copy the tls template to the tls area */
  my_memcpy(__nacl_tls_data_start(combined_area),
            TLS_TDATA_START,
            TLS_TDATA_SIZE);

  my_memset(__nacl_tls_bss_start(combined_area),
            0,
            TLS_TBSS_SIZE);
}


/* This function initializes TLS and TDB for the main thread.
 * It is always called - with and without pthreads.
 * TDB initialization for the main thread is somewhat simpler than for
 * other threads.
 */
int __pthread_initialize_minimal(size_t tdb_size) {
  /* Allocate the TLS + TDB. Note that we cannot use malloc here since the
   * library is not initialized yet.
   */

  int size = __nacl_tls_combined_size(tdb_size, 1);
  /* adapt size for sbrk */
  /* TODO(robertm): this is somewhat arbitrary - re-examnine) */
  size = (size + 15) & ~15;
  void *tls = sbrk(size);

  __nacl_tls_data_bss_initialize_from_template(tls);

  void *main_tdb = __nacl_tls_tdb_start(tls);
  /* patch the first word of the tdb to contain the "base" of the tls area */
  *(void**)main_tdb = __nacl_tls_tdb_start(tls);

  /* set GS */
  NACL_SYSCALL(tls_init)(main_tdb, tdb_size);

  /* initialize newlib's thread-specific pointer. */
  __newlib_thread_init();
  return 0;
}


int __pthread_initialize() {
  /* all we need is to have the self pointer in the TDB */
  return __pthread_initialize_minimal(sizeof(void*));
}


int __pthread_shutdown() {
  /* No shutdown is required when pthread library is not used */
  return 0;
}
