/*
 * Copyright 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_NONSFI_LINUX_LINUX_SYSCALL_DEFINES_H_
#define NATIVE_CLIENT_SRC_NONSFI_LINUX_LINUX_SYSCALL_DEFINES_H_ 1

#define CLONE_VM             0x00000100
#define CLONE_FS             0x00000200
#define CLONE_FILES          0x00000400
#define CLONE_SIGHAND        0x00000800
#define CLONE_THREAD         0x00010000
#define CLONE_SYSVSEM        0x00040000
#define CLONE_SETTLS         0x00080000

#define LINUX_SIGCHLD 17

#define FUTEX_WAIT_PRIVATE 128
#define FUTEX_WAKE_PRIVATE 129

#define LINUX_TCGETS 0x5401

#define LINUX_SA_SIGINFO 0x00000004
#define LINUX_SA_RESTART 0x10000000
#define LINUX_SA_ONSTACK 0x08000000

#endif
