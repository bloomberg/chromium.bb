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
* Native Client semaphore API
*/

#include <unistd.h>
#include <errno.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

#include "native_client/src/untrusted/pthread/pthread.h"
#include "native_client/src/untrusted/pthread/pthread_types.h"
#include "native_client/src/untrusted/pthread/semaphore.h"

/* Initialize semaphore  */
int sem_init (sem_t *sem, int pshared, unsigned int value) {
  if ((pshared) || (value > SEM_VALUE_MAX)) {
    /* we don's support shared semaphores yet */
    errno = EINVAL;
    return -1;
  }
  sem->handle = NACL_SYSCALL(sem_create)(value);
  return (sem->handle < 0);
}

int sem_destroy (sem_t *sem) {
  int retval = close(sem->handle);
  sem->handle = NC_INVALID_HANDLE;
  return retval;
}

int sem_wait (sem_t *sem) {
  return NACL_SYSCALL(sem_wait)(sem->handle);
}

int sem_post (sem_t *sem) {
  int32_t rv = NACL_SYSCALL(sem_post)(sem->handle);
  if (0 != rv) {
    errno = -rv;
    return -1;
  }
  return rv;
}
