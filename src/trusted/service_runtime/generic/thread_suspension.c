/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"


void NaClAppThreadSetSuspendState(struct NaClAppThread *natp,
                                  enum NaClSuspendState old_state,
                                  enum NaClSuspendState new_state) {
  UNREFERENCED_PARAMETER(natp);
  UNREFERENCED_PARAMETER(old_state);
  UNREFERENCED_PARAMETER(new_state);
}

void NaClUntrustedThreadsSuspendAll(struct NaClApp *nap) {
  UNREFERENCED_PARAMETER(nap);

  NaClLog(LOG_FATAL, "NaClUntrustedThreadsSuspendAll: Not implemented\n");
}

void NaClUntrustedThreadsResumeAll(struct NaClApp *nap) {
  UNREFERENCED_PARAMETER(nap);

  NaClLog(LOG_FATAL, "NaClUntrustedThreadsResumeAll: Not implemented\n");
}
