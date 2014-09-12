/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/include/minsfi_syscalls.h"

int32_t __minsfi_syscall_sysconf(uint32_t name, sfiptr_t dest_result,
                                 sfiptr_t dest_errno) {
  const MinsfiSandbox *sb = MinsfiGetActiveSandbox();
  int32_t value;

  switch (name) {
    case MINSFI_SYSCONF_PAGESIZE:
      value = sysconf(_SC_PAGESIZE);
      MinsfiCopyToSandbox(dest_result, &value, sizeof(value), sb);
      return 0;
    default:
      value = EINVAL;
      MinsfiCopyToSandbox(dest_errno, &value, sizeof(value), sb);
      return -1;
  }
}
