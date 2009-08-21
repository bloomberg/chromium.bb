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
 * Native Client condition variable API
 */

#include <unistd.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

#include "native_client/src/untrusted/pthread/pthread.h"
#include "native_client/src/untrusted/pthread/pthread_types.h"


/* TODO(gregoryd): make this static?  */
void pthread_cond_validate(pthread_cond_t* cond) {
  nc_spinlock_lock(&cond->spinlock);

  if (NC_INVALID_HANDLE == cond->handle) {
    pthread_cond_init(cond, NULL);
  }

  nc_spinlock_unlock(&cond->spinlock);
}


/*
 * Initialize condition variable COND using attributes ATTR, or use
 * the default values if later is NULL.
 */
int pthread_cond_init (pthread_cond_t *cond,
                       pthread_condattr_t *cond_attr) {
  cond->handle = NACL_SYSCALL(cond_create)();

  /* 0 for success, 1 for failure */
  return (cond->handle < 0);
}

/*
 * Destroy condition variable COND.
 */
int pthread_cond_destroy (pthread_cond_t *cond) {
  int retval;
  pthread_cond_validate(cond);
  retval = close(cond->handle);
  cond->handle = NC_INVALID_HANDLE;
  return retval;
}

/*
 * Wake up one thread waiting for condition variable COND.
 */
int pthread_cond_signal (pthread_cond_t *cond) {
  pthread_cond_validate(cond);
  return NACL_SYSCALL(cond_signal)(cond->handle);
}


int pthread_cond_broadcast (pthread_cond_t *cond) {
  pthread_cond_validate(cond);
  return NACL_SYSCALL(cond_broadcast)(cond->handle);
}

int pthread_cond_wait (pthread_cond_t *cond,
                       pthread_mutex_t *mutex) {
  pthread_cond_validate(cond);
  return NACL_SYSCALL(cond_wait)(cond->handle, mutex->mutex_handle);
}

int pthread_cond_timedwait_abs(pthread_cond_t *cond,
                               pthread_mutex_t *mutex,
                               struct timespec *abstime) {
  pthread_cond_validate(cond);
  return NACL_SYSCALL(cond_timed_wait_abs)(cond->handle,
                                           mutex->mutex_handle,
                                           abstime);
}

int nc_pthread_condvar_ctor(pthread_cond_t *cond) {
  cond->handle = NC_INVALID_HANDLE;
  cond->spinlock = 0;
  return 1;
}
