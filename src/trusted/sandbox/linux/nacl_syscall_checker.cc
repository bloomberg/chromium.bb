/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/sandbox/linux/nacl_syscall_checker.h"

SyscallChecker::~SyscallChecker() {
  // NOTE: gcc does not seem to define a default desctructor in this case
}
