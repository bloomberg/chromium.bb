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
 * Native Client mutex implementation
 */

#include <errno.h>
#include <unistd.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

#include "native_client/src/untrusted/pthread/pthread.h"
#include "native_client/src/untrusted/pthread/pthread_types.h"

/* Mutex functions */

static int nc_thread_mutex_init(pthread_mutex_t *mutex) {
  mutex->owner_thread_id = NACL_PTHREAD_ILLEGAL_THREAD_ID;
  mutex->recursion_counter = 0;
  mutex->mutex_handle = NACL_SYSCALL(mutex_create)();

  /* 0 for success, 1 for failure */
  return (mutex->mutex_handle < 0);
}

int pthread_mutex_validate(pthread_mutex_t *mutex) {
  int rv = 0;
  nc_spinlock_lock(&mutex->spinlock);

  if (NC_INVALID_HANDLE == mutex->mutex_handle) {
    /* Mutex_type is set by the initializer */
    rv = nc_thread_mutex_init(mutex);
  }

  nc_spinlock_unlock(&mutex->spinlock);
  return rv;
}

int pthread_mutex_init (pthread_mutex_t *mutex,
                        pthread_mutexattr_t *mutex_attr) {
  if (mutex_attr != NULL) {
    mutex->mutex_type = mutex_attr->kind;
  } else {
    mutex->mutex_type = PTHREAD_MUTEX_FAST_NP;
  }
  return nc_thread_mutex_init(mutex);
}

int pthread_mutex_destroy (pthread_mutex_t *mutex) {
  int retval;
  pthread_mutex_validate(mutex);
  if (NACL_PTHREAD_ILLEGAL_THREAD_ID != mutex->owner_thread_id) {
    /* the mutex is still locked - cannot destroy */
    return EBUSY;
  }
  retval = close(mutex->mutex_handle);
  mutex->mutex_handle = NC_INVALID_HANDLE;
  mutex->owner_thread_id = NACL_PTHREAD_ILLEGAL_THREAD_ID;
  mutex->recursion_counter = 0;
  return retval;
}

static int nc_thread_mutex_lock(pthread_mutex_t *mutex, int try_only) {
  int rv;
  pthread_mutex_validate(mutex);
  /*
   * Checking mutex's owner thread id without synchronization is safe:
   * - We are checking whether the owner's id is equal to the current thread id,
   * and this can happen only if the current thread is actually the owner,
   * otherwise the owner id will hold an illegal value or an id of a different
   * thread.
   * - The value we read from the owner id cannot be a combination of two
   * values, since properly aligned 32-bit values are updated
   * by a single machine instruction, so the current thread can only read
   * a value that it or some other thread wrote.
   * - Cache is not an issue since a thread will always update its own cache
   * when unlocking a mutex (see pthread_mutex_unlock implementation).
   */
  if ((mutex->mutex_type != PTHREAD_MUTEX_FAST_NP) &&
      (pthread_self() == mutex->owner_thread_id)) {
    if (mutex->mutex_type == PTHREAD_MUTEX_ERRORCHECK_NP) {
      return EDEADLK;
    } else {
      /* This thread already owns the mutex */
      ++mutex->recursion_counter;
      return 0;
    }
  }
  if (try_only) {
    rv = NACL_SYSCALL(mutex_trylock)(mutex->mutex_handle);
  } else {
    rv = NACL_SYSCALL(mutex_lock)(mutex->mutex_handle);
  }
  if (rv) {
    return rv;
  }

  mutex->owner_thread_id = pthread_self();
  mutex->recursion_counter = 1;

  return 0;
}

int pthread_mutex_trylock (pthread_mutex_t *mutex) {
  return nc_thread_mutex_lock(mutex, 1);
}

int pthread_mutex_lock (pthread_mutex_t *mutex) {
  return nc_thread_mutex_lock(mutex, 0);
}

int pthread_mutex_unlock (pthread_mutex_t *mutex) {
  pthread_mutex_validate(mutex);
  if (mutex->mutex_type != PTHREAD_MUTEX_FAST_NP) {
    if ((PTHREAD_MUTEX_RECURSIVE_NP == mutex->mutex_type) &&
        (0 != (--mutex->recursion_counter))) {
      /*
       * We assume that this thread owns the lock
       * (no verification for recursive locks),
       * so just decrement the counter, this thread is still the owner
       */
      return 0;
    }
    if ((PTHREAD_MUTEX_ERRORCHECK_NP == mutex->mutex_type) &&
        (pthread_self() != mutex->owner_thread_id)) {
      /* error - releasing a mutex that's free or owned by another thread */
      return EPERM;
    }
  }
  mutex->owner_thread_id = NACL_PTHREAD_ILLEGAL_THREAD_ID;
  mutex->recursion_counter = 0;
  return NACL_SYSCALL(mutex_unlock)(mutex->mutex_handle);
}

/*
 * NOTE(sehr): pthread_once needs to be defined in the same module as contains
 * the mutex definitions, so that it overrides the weak symbol in the libstdc++
 * library.  Otherwise we get calls through address zero.
 */
int pthread_once(pthread_once_t* __once_control,
                  void (*__init_routine) (void)) {
/*
 * NOTE(gregoryd): calling pthread_once from __init_routine providing the same
 * __once_control argument is an error and will cause a deadlock
 */
  AtomicWord res;
  res = AtomicIncrement(&__once_control->done, 0);
  if (!res) {
    /* not done yet */
    pthread_mutex_lock(&__once_control->lock);
    res = AtomicIncrement(&__once_control->done, 0);
    if (!res) {
      /* still not done - but this time we own the lock */
      (*__init_routine)();
      AtomicIncrement(&__once_control->done, 1);

    }
    pthread_mutex_unlock(&__once_control->lock);
  }
  return 0;
}

int pthread_mutexattr_init(pthread_mutexattr_t *attr) {
  /* The default mutex type is non-recursive */
  attr->kind = PTHREAD_MUTEX_FAST_NP;
  return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t *attr) {
  /* nothing to do */
  return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t *attr,
                              int kind) {
  if ((kind == PTHREAD_MUTEX_FAST_NP) ||
      (kind == PTHREAD_MUTEX_RECURSIVE_NP) ||
      (kind == PTHREAD_MUTEX_ERRORCHECK_NP)) {
    attr->kind = kind;
  } else {
    /* unknown kind */
    return -1;
  }
  return 0;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr,
                              int *kind) {
  *kind = attr->kind;
  return 0;
}
