/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Native Client condition variable API
 */

#include <errno.h>
#include <unistd.h>

#include "native_client/src/untrusted/nacl/nacl_irt.h"
#include "native_client/src/untrusted/pthread/pthread.h"
#include "native_client/src/untrusted/pthread/pthread_internal.h"
#include "native_client/src/untrusted/pthread/pthread_types.h"

static int nc_thread_cond_init(pthread_cond_t *cond,
                               const pthread_condattr_t *cond_attr) {
  return __nc_irt_cond.cond_create(&cond->handle);
}

/* TODO(gregoryd): make this static?  */
void pthread_cond_validate(pthread_cond_t *cond) {
  if (nc_token_acquire(&cond->token)) {
    nc_thread_cond_init(cond, NULL);
    nc_token_release(&cond->token);
  }
}


/*
 * Initialize condition variable COND using attributes ATTR, or use
 * the default values if later is NULL.
 */
int pthread_cond_init(pthread_cond_t *cond,
                      const pthread_condattr_t *cond_attr) {
  int retval;
  nc_token_init(&cond->token, 1);
  retval = nc_thread_cond_init(cond, cond_attr);
  nc_token_release(&cond->token);
  if (0 != retval)
    return EAGAIN;
  return 0;
}

/*
 * Destroy condition variable COND.
 */
int pthread_cond_destroy(pthread_cond_t *cond) {
  int retval;
  pthread_cond_validate(cond);
  retval = __nc_irt_cond.cond_destroy(cond->handle);
  cond->handle = NC_INVALID_HANDLE;
  return retval;
}

/*
 * Wake up one thread waiting for condition variable COND.
 */
int pthread_cond_signal(pthread_cond_t *cond) {
  pthread_cond_validate(cond);
  return __nc_irt_cond.cond_signal(cond->handle);
}

int pthread_cond_broadcast(pthread_cond_t *cond) {
  pthread_cond_validate(cond);
  return __nc_irt_cond.cond_broadcast(cond->handle);
}

int pthread_cond_wait(pthread_cond_t *cond,
                      pthread_mutex_t *mutex) {
  pthread_cond_validate(cond);
  int retval = __nc_irt_cond.cond_wait(cond->handle, mutex->mutex_handle);
  if (retval == 0) {
    mutex->owner_thread_id = pthread_self();
    mutex->recursion_counter = 1;
  }
  return retval;
}

int pthread_cond_timedwait_abs(pthread_cond_t *cond,
                               pthread_mutex_t *mutex,
                               const struct timespec *abstime) {
  pthread_cond_validate(cond);
  int retval = __nc_irt_cond.cond_timed_wait_abs(cond->handle,
                                                 mutex->mutex_handle,
                                                 abstime);
  if (retval == 0 || retval == ETIMEDOUT) {
    mutex->owner_thread_id = pthread_self();
    mutex->recursion_counter = 1;
  }
  return retval;
}

int nc_pthread_condvar_ctor(pthread_cond_t *cond) {
  nc_token_init(&cond->token, 0);
  cond->handle = NC_INVALID_HANDLE;
  return 1;
}
