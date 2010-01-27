/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
