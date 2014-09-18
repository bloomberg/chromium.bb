/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/include/minsfi_syscalls.h"

int32_t __minsfi_syscall_exit(int32_t exit_code) {
  MinsfiSandbox *sb = MinsfiGetActiveSandbox();
  ((volatile MinsfiSandbox*) sb)->exit_code = exit_code;
  longjmp(sb->exit_env, 1);
}
