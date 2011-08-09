/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
* Native Client semaphore API
*/

#include <unistd.h>
#include <errno.h>

#include "native_client/src/untrusted/irt/irt_interfaces.h"
#include "native_client/src/untrusted/nacl/nacl_irt.h"

#include "native_client/src/untrusted/pthread/pthread.h"
#include "native_client/src/untrusted/pthread/pthread_internal.h"
#include "native_client/src/untrusted/pthread/pthread_types.h"
#include "native_client/src/untrusted/pthread/semaphore.h"

/* Initialize semaphore  */
int sem_init(sem_t *sem, int pshared, unsigned int value) {
  if (NULL == sem) {
    errno = EINVAL;
    return -1;
  }
  if (pshared) {
    /* We don't support shared semaphores yet. */
    errno = ENOSYS;
    return -1;
  }
  if (value > SEM_VALUE_MAX) {
    errno = EINVAL;
    return -1;
  }
  int err = __nc_irt_sem.sem_create(&sem->handle, value);
  if (0 != err) {
    errno = err;
    return -1;
  }
  return 0;
}

int sem_destroy(sem_t *sem) {
  if (NULL == sem || NC_INVALID_HANDLE == sem->handle) {
    errno = EINVAL;
    return -1;
  }
  int rv = __nc_irt_sem.sem_destroy(sem->handle);
  sem->handle = NC_INVALID_HANDLE;
  if (0 != rv) {
    errno = EINVAL;
    return -1;
  }
  return rv;
}

int sem_wait(sem_t *sem) {
  if (NULL == sem || NC_INVALID_HANDLE == sem->handle) {
    errno = EINVAL;
    return -1;
  }
  int err = __nc_irt_sem.sem_wait(sem->handle);
  if (0 != err) {
    errno = err;
    return -1;
  }
  return 0;
}

int sem_post(sem_t *sem) {
  if (NULL == sem || NC_INVALID_HANDLE == sem->handle) {
    errno = EINVAL;
    return -1;
  }
  int err = __nc_irt_sem.sem_post(sem->handle);
  if (0 != err) {
    errno = err;
    return -1;
  }
  return 0;
}
