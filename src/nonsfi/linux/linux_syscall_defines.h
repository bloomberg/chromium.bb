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

#define LINUX_S_IFMT   0170000
#define LINUX_S_IFDIR  0040000
#define LINUX_S_IFCHR  0020000
#define LINUX_S_IFBLK  0060000
#define LINUX_S_IFREG  0100000
#define LINUX_S_IFIFO  0010000
#define LINUX_S_IFLNK  0120000
#define LINUX_S_IFSOCK 0140000

#define LINUX_S_IRUSR 0400
#define LINUX_S_IWUSR 0200
#define LINUX_S_IXUSR 0100

#endif
