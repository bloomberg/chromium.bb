/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Native Client condition variable API
 */

#include <errno.h>
#include <unistd.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

#include "native_client/src/untrusted/pthread/pthread.h"
#include "native_client/src/untrusted/pthread/pthread_types.h"

static int nc_thread_cond_init(pthread_cond_t *cond,
                               pthread_condattr_t *cond_attr) {
  cond->handle = NACL_SYSCALL(cond_create)();

  /* 0 for success, 1 for failure */
  return (cond->handle < 0);
}

/* TODO(gregoryd): make this static?  */
void pthread_cond_validate(pthread_cond_t* cond) {
  nc_spinlock_lock(&cond->spinlock);

  if (NC_INVALID_HANDLE == cond->handle) {
    nc_thread_cond_init(cond, NULL);
  }

  nc_spinlock_unlock(&cond->spinlock);
}


/*
 * Initialize condition variable COND using attributes ATTR, or use
 * the default values if later is NULL.
 */
int pthread_cond_init (pthread_cond_t *cond,
                       pthread_condattr_t *cond_attr) {
  cond->spinlock = 0;
  if (0 != nc_thread_cond_init(cond, cond_attr))
    return EAGAIN;
  return 0;
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
  return -NACL_GC_WRAP_SYSCALL(NACL_SYSCALL(cond_signal)(cond->handle));
}

int pthread_cond_broadcast (pthread_cond_t *cond) {
  pthread_cond_validate(cond);
  return -NACL_GC_WRAP_SYSCALL(NACL_SYSCALL(cond_broadcast)(cond->handle));
}

int pthread_cond_wait (pthread_cond_t *cond,
                       pthread_mutex_t *mutex) {
  pthread_cond_validate(cond);
  int retval = -NACL_GC_WRAP_SYSCALL(
          NACL_SYSCALL(cond_wait)(cond->handle, mutex->mutex_handle));
  if (retval == 0) {
    mutex->owner_thread_id = pthread_self();
    mutex->recursion_counter = 1;
  }
  return retval;
}

int pthread_cond_timedwait_abs(pthread_cond_t *cond,
                               pthread_mutex_t *mutex,
                               struct timespec *abstime) {
  pthread_cond_validate(cond);
  int retval = -NACL_GC_WRAP_SYSCALL(
            NACL_SYSCALL(cond_timed_wait_abs)(cond->handle,
                                              mutex->mutex_handle,
                                              abstime));
  if (retval == 0) {
    mutex->owner_thread_id = pthread_self();
    mutex->recursion_counter = 1;
  }
  return retval;
}

int nc_pthread_condvar_ctor(pthread_cond_t *cond) {
  cond->handle = NC_INVALID_HANDLE;
  cond->spinlock = 0;
  return 1;
}
