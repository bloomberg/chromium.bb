/*
 * Copyright 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_PUBLIC_LINUX_SYSCALLS_SYS_PRCTL_H_
#define NATIVE_CLIENT_SRC_PUBLIC_LINUX_SYSCALLS_SYS_PRCTL_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int prctl(int option, uintptr_t arg2, uintptr_t arg3,
          uintptr_t arg4, uintptr_t arg5);

#define PR_GET_NAME 16

#ifdef __cplusplus
}
#endif

#endif
