/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_PTHREAD_FUTEX_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_PTHREAD_FUTEX_H_ 1

struct timespec;

void __nc_futex_init(void);
void __nc_futex_thread_exit(void);

int __nc_futex_wait(volatile int *addr, int value,
                    const struct timespec *abstime);
int __nc_futex_wake(volatile int *addr, int nwake, int *count);

#endif
