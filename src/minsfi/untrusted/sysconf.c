/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/include/minsfi_syscalls.h"

long sysconf(int name) {
  int32_t ret_val, result;
  ret_val = __minsfi_syscall_sysconf(name, &result, &errno);
  return (ret_val == 0) ? result : -1;
}
