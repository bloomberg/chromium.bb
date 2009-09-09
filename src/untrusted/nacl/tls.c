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


static char * __nacl_tls_align(void* p) {
  return (char *)((((int32_t) p) + TLS_ALIGNMENT - 1) & ~(TLS_ALIGNMENT - 1));
}


size_t __nacl_tls_combined_size(size_t tdb_size) {
  return TLS_TDATA_SIZE + TLS_TBSS_SIZE + tdb_size + TLS_ALIGNMENT - 1;
}


/* the organization of the tls block (aka combined tls area) in nacl is
 *
 *  [unaligned fluff]
 *  TLS_DATA
 *  TLS_BSS
 *  TDB
 *
 * The size of TLS_DATA TLS_BSS are multiples of TLS_ALIGNMENT enforced
 * by the linker
 */

static void* __nacl_tls_data_start(void* p) {
  return __nacl_tls_align(p);
}


static void* __nacl_tls_bss_start(void* p) {
  return __nacl_tls_align(p) + TLS_TDATA_SIZE;
}


void* __nacl_tls_tdb_start(void* p) {
  return __nacl_tls_align(p) + TLS_TDATA_SIZE + TLS_TBSS_SIZE;
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
  void *tls = sbrk(__nacl_tls_combined_size(tdb_size));

  __nacl_tls_data_bss_initialize_from_template(tls);

  void *main_tdb = __nacl_tls_tdb_start(tls);

  *(void**)main_tdb = main_tdb;

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
