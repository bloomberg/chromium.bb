/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_NONSFI_LINUX_LINUX_SYSCALLS_H_
#define NATIVE_CLIENT_SRC_NONSFI_LINUX_LINUX_SYSCALLS_H_ 1

/* Definitions of Linux syscall numbers. */

#if defined(__i386__)

# define __NR_exit               1
# define __NR_read               3
# define __NR_write              4
# define __NR_open               5
# define __NR_close              6
# define __NR_dup                41
# define __NR_pipe               42
# define __NR_dup2               63
# define __NR_gettimeofday       78
# define __NR_munmap             91
# define __NR_wait4              114
# define __NR_clone              120
# define __NR_mprotect           125
# define __NR__llseek            140
# define __NR_sched_yield        158
# define __NR_nanosleep          162
# define __NR_pread64            180
# define __NR_pwrite64           181
# define __NR_mmap2              192
# define __NR_futex              240
# define __NR_set_thread_area    243
# define __NR_exit_group         252
# define __NR_clock_gettime      265
# define __NR_clock_getres       266

#elif defined(__arm__)

# define __NR_exit               1
# define __NR_read               3
# define __NR_write              4
# define __NR_open               5
# define __NR_close              6
# define __NR_dup                41
# define __NR_pipe               42
# define __NR_dup2               63
# define __NR_gettimeofday       78
# define __NR_munmap             91
# define __NR_wait4              114
# define __NR_clone              120
# define __NR_mprotect           125
# define __NR__llseek            140
# define __NR_sched_yield        158
# define __NR_nanosleep          162
# define __NR_pread64            180
# define __NR_pwrite64           181
# define __NR_mmap2              192
# define __NR_futex              240
# define __NR_exit_group         248
# define __NR_clock_gettime      263
# define __NR_clock_getres       264
# define __NR_ARM_set_tls        0xf0005

#else
# error Unsupported architecture
#endif

#endif
