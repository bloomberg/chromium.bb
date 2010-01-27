/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Native Client condition variable API
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_PTHREAD_CONDVAR_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_PTHREAD_CONDVAR_H_ 1

/* TODO(gregoryd): remove the header file dependencies here */
#include <sys/types.h>
#include "nc_system.h"
#include "pthread.h"


int nc_condvar_create(nc_cond_t *cv);
int nc_condvar_destroy(nc_cond_t *cv);
int nc_condvar_wait(nc_cond_t *cv, pthread_mutex_t *mutex);
int nc_condvar_timedwait_abs(nc_cond_t *cv,
                             pthread_mutex_t *mutex,
                             struct timespec *abstime);
int nc_condvar_signal(nc_cond_t* cv);
int nc_condvar_broadcast(nc_cond_t* cv);

#endif  /* NATIVE_CLIENT_SRC_UNTRUSTED_PTHREAD_CONDVAR_H_ */
