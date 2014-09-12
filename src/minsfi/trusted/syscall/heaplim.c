/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/include/minsfi_syscalls.h"

int32_t __minsfi_syscall_heaplim(sfiptr_t dest_lower_limit,
                                 sfiptr_t dest_upper_limit) {
  const MinsfiSandbox *sb = MinsfiGetActiveSandbox();

  sfiptr_t val_lower_limit = sb->mem_layout.heap.offset;
  sfiptr_t val_upper_limit = val_lower_limit + sb->mem_layout.heap.length;

  MinsfiCopyToSandbox(dest_lower_limit, &val_lower_limit,
                      sizeof(val_lower_limit), sb);
  MinsfiCopyToSandbox(dest_upper_limit, &val_upper_limit,
                      sizeof(val_upper_limit), sb);

  return 0;
}
