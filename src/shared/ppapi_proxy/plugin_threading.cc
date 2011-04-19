/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <pthread.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/ppruntime.h"


/*
 * This provides a default definition that is overridden in the IRT.
 * TODO(mseaborn): Remove this when PPAPI is only supported via the IRT.
 * See http://code.google.com/p/nativeclient/issues/detail?id=1691
 */

static int thread_create(uintptr_t *tid,
                         void (*func)(void *thread_argument),
                         void *thread_argument) {
  /*
   * We know that newlib and glibc use a small pthread_t type, so we
   * do not need to wrap pthread_t values.
   */
  NACL_COMPILE_TIME_ASSERT(sizeof(pthread_t) == sizeof(uintptr_t));

  return pthread_create((pthread_t *) tid, NULL,
                        (void *(*)(void *thread_argument)) func,
                        thread_argument);
}

static int thread_join(uintptr_t tid) {
  return pthread_join((pthread_t) tid, NULL);
}

const static struct PP_ThreadFunctions thread_funcs = {
  thread_create,
  thread_join
};

void PpapiPluginRegisterDefaultThreadCreator() {
  PpapiPluginRegisterThreadCreator(&thread_funcs);
}
