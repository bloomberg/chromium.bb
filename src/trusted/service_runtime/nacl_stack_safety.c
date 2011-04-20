/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_stack_safety.h"

#if NACL_WINDOWS

#include "native_client/src/trusted/service_runtime/nacl_tls.h"
#include "native_client/src/shared/platform/nacl_log.h"

/* paranoia default */
THREAD int32_t nacl_thread_on_safe_stack = 0;

void NaClStackSafetyNowOnUntrustedStack(void) {
  nacl_thread_on_safe_stack = 0;
}

void NaClStackSafetyNowOnTrustedStack(void) {
  nacl_thread_on_safe_stack = 1;
}

#elif NACL_OSX || NACL_LINUX

void NaClStackSafetyNowOnUntrustedStack(void) {
  return;
}

void NaClStackSafetyNowOnTrustedStack(void) {
  return;
}

#else
# error "What OS are we on?!?"
#endif
