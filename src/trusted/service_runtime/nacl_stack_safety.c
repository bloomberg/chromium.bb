/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_stack_safety.h"

#if NACL_WINDOWS

#include <windows.h>

#include "native_client/src/trusted/service_runtime/nacl_tls.h"
#include "native_client/src/shared/platform/nacl_log.h"

int32_t nacl_thread_on_safe_stack_tls_index;

void NaClStackSafetyInit(void) {
  if (TLS_OUT_OF_INDEXES ==
      (nacl_thread_on_safe_stack_tls_index = TlsAlloc())) {
    NaClLog(LOG_FATAL,
            "NaClStackSafetyInit failed: out of TLS indices so early?!?\n");
  }
  TlsSetValue(nacl_thread_on_safe_stack_tls_index, (void *) 0);
}

void NaClStackSafetyFini(void) {
  ;
  /*
   * we could invoke
   *
   * (void) TlsFree(nacl_thread_on_safe_stack_tls_index);
   *
   * here, and may wish to do so later for e.g. fuzzing where we will
   * need to spin up many NaCl modules w/o creating may sel_ldr
   * processes.
   */
}

void NaClStackSafetyNowOnUntrustedStack(void) {
  TlsSetValue(nacl_thread_on_safe_stack_tls_index, (void *) 0);
}

void NaClStackSafetyNowOnTrustedStack(void) {
  TlsSetValue(nacl_thread_on_safe_stack_tls_index, (void *) 1);
}

#elif NACL_OSX || NACL_LINUX

void NaClStackSafetyInit(void) {
  ;
}

void NaClStackSafetyFini(void) {
  ;
}

void NaClStackSafetyNowOnUntrustedStack(void) {
  return;
}

void NaClStackSafetyNowOnTrustedStack(void) {
  return;
}

#else
# error "What OS are we on?!?"
#endif
