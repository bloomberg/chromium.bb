/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_PPAPI_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_PPAPI_H_ 1

#include <stddef.h>

#include "ppapi/c/ppp.h"

struct PP_StartFunctions {
  int32_t (*PPP_InitializeModule)(PP_Module module_id,
                                  PPB_GetInterface get_browser_interface);
  void (*PPP_ShutdownModule)();
  const void *(*PPP_GetInterface)(const char *interface_name);
};

struct PP_ThreadFunctions {
  /*
   * This is a cut-down version of pthread_create()/pthread_join().
   * We omit thread creation attributes and the thread's return value.
   *
   * We use uintptr_t as the thread ID type because pthread_t is not
   * part of the stable ABI; a user thread library might choose an
   * arbitrary size for its own pthread_t.
   */
  int (*thread_create)(uintptr_t *tid,
                       void (*func)(void *thread_argument),
                       void *thread_argument);
  int (*thread_join)(uintptr_t tid);
};

#endif
