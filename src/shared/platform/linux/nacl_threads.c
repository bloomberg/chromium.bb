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
 * NaCl Server Runtime threads implementation layer.
 */

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <pthread.h>
#include <limits.h>
/*
 * PTHREAD_STACK_MIN should come from pthread.h as documented, but is
 * actually pulled in by limits.h.
 */

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"

#if NACL_KERN_STACK_SIZE < PTHREAD_STACK_MIN
# error "NaCl service runtime stack size is smaller than PTHREAD_STACK_MIN"
#endif

/*
 * Even if ctor fails, it should be okay -- and required -- to invoke
 * the dtor on it.
 */
int NaClThreadCtor(struct NaClThread  *ntp,
                   void               (*start_fn)(void *),
                   void               *state,
                   size_t             stack_size) {
  pthread_attr_t  attr;
  int             code;
  int             rv;
  char            err_string[1024];

  rv = 0;

  if (stack_size < PTHREAD_STACK_MIN) {
    stack_size = PTHREAD_STACK_MIN;
  }
  if (0 != (code = pthread_attr_init(&attr))) {
    NaClLog(LOG_ERROR,
            "NaClThreadCtor: pthread_atr_init returned %d",
            code);
    goto done;
  }
  if (0 != (code = pthread_attr_setstacksize(&attr, stack_size))) {
    NaClLog(LOG_ERROR,
            "NaClThreadCtor: pthread_attr_setstacksize returned %d (%s)",
            code,
            (strerror_r(code, err_string, sizeof err_string) == 0
             ? err_string
             : "UNKNOWN"));
    goto done_attr_dtor;
  }
  if (0 !=
      (code = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))) {
    NaClLog(LOG_ERROR,
            "nacl_thread: pthread_attr_setdetachstate returned %d (%s)",
            code,
            (strerror_r(code, err_string, sizeof err_string) == 0
             ? err_string
             : "UNKNOWN"));
    goto done_attr_dtor;
  }
  if (0 != (code = pthread_create(&ntp->tid,
                                  &attr,
                                  (void *(*)(void *)) start_fn,
                                  state))) {
    NaClLog(LOG_ERROR,
            "nacl_thread: pthread_create returned %d (%s)",
            code,
            (strerror_r(code, err_string, sizeof err_string) == 0
             ? err_string
             : "UNKNOWN"));
    goto done_attr_dtor;
  }
  rv = 1;
 done_attr_dtor:
  (void) pthread_attr_destroy(&attr);  /* often a noop */
 done:
  return rv;
}

void NaClThreadDtor(struct NaClThread *ntp) {
  /*
   * The threads that we create are not joinable, and we cannot tell
   * when they are truly gone.  Fortunately, the threads themselves
   * and the underlying thread library are responsible for ensuring
   * that resources such as the thread stack are properly released.
   */
}

void NaClThreadExit(void) {
  pthread_exit((void *) 0);
}

void NaClThreadKill(struct NaClThread *target) {
  pthread_kill(target->tid, SIGKILL);
}

int NaClTsdKeyCreate(struct NaClTsdKey  *tsdp) {
  int errcode;

  errcode = pthread_key_create(&tsdp->key, (void (*)(void *)) NULL);
  /* returns zero on success, error code on failure */
  if (0 != errcode) {
    NaClLog(LOG_ERROR,
            "NaClTsdKeyCreate: could not create new key, error code %d",
            errcode);
  }
  return 0 == errcode;
}

int NaClTsdSetSpecific(struct NaClTsdKey  *tsdp,
                       void const         *ptr) {
  int errcode;

  errcode = pthread_setspecific(tsdp->key, ptr);
  /* returns zero on success, error code on failure */
  if (0 != errcode) {
    NaClLog(LOG_ERROR,
            "NaClTsdSetSpecific: could not set new value, error code %d",
            errcode);
  }
  return 0 == errcode;
}

void *NaClTsdGetSpecific(struct NaClTsdKey  *tsdp) {
  return pthread_getspecific(tsdp->key);
}

uint32_t NaClThreadId(void) {
  return (uintptr_t) pthread_self();
}
