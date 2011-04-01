/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_NACL_NACL_THREAD_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_NACL_NACL_THREAD_H_ 1

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct timespec;  /* Forward declaration.  */

/*
 * Consistent with the style of <pthread.h> functions, all these functions
 * return zero for success or an errno code for failure.
 */

extern int nacl_thread_create(void *start_user_address, void *stack,
                              void *tdb, size_t tdb_size);
#ifdef __GNUC__
extern void nacl_thread_exit(int32_t *stack_flag) __attribute__((__noreturn__));
#else
extern void nacl_thread_exit(int32_t *stack_flag);
#endif
/*
 * The argument should be a NICE_* constant from <sys/nacl_nice.h>.
 */
extern int nacl_thread_nice(const int nice);

extern int nacl_mutex_create(int *mutex_handle);
extern int nacl_mutex_lock(int mutex_handle);
extern int nacl_mutex_unlock(int mutex_handle);
extern int nacl_mutex_trylock(int mutex_handle);

extern int nacl_cond_create(int *cond_handle);
extern int nacl_cond_signal(int cond_handle);
extern int nacl_cond_broadcast(int cond_handle);
extern int nacl_cond_wait(int cond_handle, int mutex_handle);
extern int nacl_cond_timed_wait_abs(int cond_handle, int mutex_handle,
                                    const struct timespec *abstime);

extern int nacl_sem_create(int *sem_handle, int32_t value);
extern int nacl_sem_wait(int sem_handle);
extern int nacl_sem_post(int sem_handle);

#ifdef __cplusplus
}
#endif

#endif  /* NATIVE_CLIENT_SRC_UNTRUSTED_NACL_NACL_THREAD_H_ */
