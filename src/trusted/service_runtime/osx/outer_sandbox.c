/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <sandbox.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/outer_sandbox.h"


/* This is a small subset of Chromium's chrome/common/common.sb. */
const char *sandbox_profile =
  "(version 1)"
  "(deny default)"
  /*
   * This allows abort() to work.  Without this, abort()'s raise()
   * syscall fails, and its attempt to die by using an undefined
   * instruction hangs the process.  See http://crbug.com/20370
   */
  "(allow signal (target self))"
  /*
   * Allow use of semaphores: sem_init() etc.  This is required on
   * OS X 10.6 but not on 10.5.
   */
  "(allow ipc-posix-sem)"
  /*
   * Allow shared memory segments to be created: shm_open() etc.  This
   * is required on OS X 10.6 but not on 10.5.
   */
  "(allow ipc-posix-shm)";


void NaClEnableOuterSandbox(void) {
  char *error;
  int rc = sandbox_init(sandbox_profile, 0, &error);
  if (rc != 0) {
    NaClLog(LOG_FATAL, "Failed to initialise Mac OS X sandbox: %s\n", error);
  }
}
