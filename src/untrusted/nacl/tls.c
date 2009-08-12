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

extern int __pthread_initialize() __attribute__ ((weak));
extern int __pthread_shutdown() __attribute__ ((weak));
extern void __newlib_thread_init();  /* defined in newlib */

/* TODO(gregoryd) - consider using real memcpy and memset */
static void __pthread_memcpy(void* dest, const void* src, size_t size) {
  /* TODO(gregoryd): optimize to use DWORDs instead of bytes */
  size_t i;

  for (i = 0; i < size; ++i) {
    *((char*)dest + i) = *((char*)src + i);
  }
}

void __pthread_memset(void* s, int c, size_t size) {
  size_t i;
  for (i = 0; i < size; ++i) {
    *((char*)s + i) = (char) c;
  }
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
  void *tls = sbrk(TLS_TDATA_SIZE + TLS_TBSS_SIZE + tdb_size + TLS_ALIGNMENT);
  tls = (void*)(((int32_t)tls + TLS_ALIGNMENT - 1) & ~(TLS_ALIGNMENT - 1));

  /* copy the tls template to the tls area */
  __pthread_memcpy(tls, TLS_TDATA_START, TLS_TDATA_SIZE);
  __pthread_memset(tls + TLS_TDATA_SIZE, 0, TLS_TBSS_SIZE);

  /* Initialize the TDB - main thread has less stuff */
  void *main_tdb = tls + TLS_TDATA_SIZE + TLS_TBSS_SIZE;
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
