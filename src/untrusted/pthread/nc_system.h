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
 * This file contains definitions shared between the user mode libararies
 * and the service runtime.
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_PTHREAD_NC_SYSTEM_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_PTHREAD_NC_SYSTEM_H_ 1

#include <stdint.h>

#if __cplusplus
extern "C" {
#endif

struct timespec;

/* threads */
extern void __nacl_exit_thread(int32_t *stack_flag);
extern int __nacl_create_thread(void *start_user_address,
                                void *stack,
                                void *tdb,
                                size_t tdb_size);

/* mutex */
extern int __nacl_create_mutex();
extern int __nacl_lock_mutex(int mutex);
extern int __nacl_unlock_mutex(int mutex);
extern int __nacl_try_lock_mutex(int mutex);

/* condvar */
extern int __nacl_create_condvar();
extern int __nacl_wait_condvar(int cv, int mutex);
extern int __nacl_signal_condvar(int cv);
extern int __nacl_broadcast_condvar(int cv);
extern int __nacl_absolute_timed_wait_condvar(int condvar,
                                              int mutex,
                                              struct timespec *abstime);

/* semaphore */
extern int __nacl_create_sem(int32_t value);
extern int __nacl_wait_sem(int sem);
extern int __nacl_post_sem(int sem);

#if __cplusplus
}
#endif


#endif  /* NATIVE_CLIENT_SRC_UNTRUSTED_PTHREAD_NC_SYSTEM_H_ */
