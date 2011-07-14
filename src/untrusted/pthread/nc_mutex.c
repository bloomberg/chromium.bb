/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Native Client mutex implementation
 */

#include <errno.h>
#include <unistd.h>

#include "native_client/src/untrusted/nacl/nacl_irt.h"
#include "native_client/src/untrusted/pthread/pthread.h"
#include "native_client/src/untrusted/pthread/pthread_internal.h"
#include "native_client/src/untrusted/pthread/pthread_types.h"

struct nacl_irt_mutex __nc_irt_mutex; /* Set up in __pthread_initialize. */

/* Mutex functions */

static int nc_thread_mutex_init(pthread_mutex_t *mutex) {
  mutex->owner_thread_id = NACL_PTHREAD_ILLEGAL_THREAD_ID;
  mutex->recursion_counter = 0;
  return __nc_irt_mutex.mutex_create(&mutex->mutex_handle);
}

int pthread_mutex_validate(pthread_mutex_t *mutex) {
  int rv = 0;

  if (nc_token_acquire(&mutex->token)) {
    /* Mutex_type was set by pthread_mutex_init */
    rv = nc_thread_mutex_init(mutex);
    nc_token_release(&mutex->token);
  }

  return rv;
}

int pthread_mutex_init (pthread_mutex_t *mutex,
                        pthread_mutexattr_t *mutex_attr) {
  nc_token_init(&mutex->token);
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
  retval = __nc_irt_mutex.mutex_destroy(mutex->mutex_handle);
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
    rv = __nc_irt_mutex.mutex_trylock(mutex->mutex_handle);
  } else {
    rv = __nc_irt_mutex.mutex_lock(mutex->mutex_handle);
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
  return __nc_irt_mutex.mutex_unlock(mutex->mutex_handle);
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
  volatile AtomicInt32* pdone = &__once_control->done;
  if (*pdone == 0) {
    /* not done yet */
    pthread_mutex_lock(&__once_control->lock);
    if (*pdone == 0) {
      /* still not done - but this time we own the lock */
      (*__init_routine)();

      /* GCC intrinsic; see:
       * http://gcc.gnu.org/onlinedocs/gcc/Atomic-Builtins.html.
       * The x86-{32,64} compilers generate inline code.  The ARM
       * implementation is external: stubs/intrinsics_arm.S.
       */
      __sync_fetch_and_add(pdone, 1);
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
