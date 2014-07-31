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
# define __NR_link               9
# define __NR_unlink             10
# define __NR_chdir              12
# define __NR_chmod              15
# define __NR_getpid             20
# define __NR_access             33
# define __NR_rename             38
# define __NR_mkdir              39
# define __NR_rmdir              40
# define __NR_dup                41
# define __NR_pipe               42
# define __NR_ioctl              54
# define __NR_dup2               63
# define __NR_gettimeofday       78
# define __NR_symlink            83
# define __NR_readlink           85
# define __NR_munmap             91
# define __NR_socketcall         102
# define __NR_wait4              114
# define __NR_clone              120
# define __NR_mprotect           125
# define __NR__llseek            140
# define __NR_sched_yield        158
# define __NR_nanosleep          162
# define __NR_poll               168
# define __NR_prctl              172
# define __NR_rt_sigaction       174
# define __NR_pread64            180
# define __NR_pwrite64           181
# define __NR_getcwd             183
# define __NR_mmap2              192
# define __NR_truncate64         193
# define __NR_stat64             195
# define __NR_lstat64            196
# define __NR_fstat64            197
# define __NR_fcntl64            221
# define __NR_futex              240
# define __NR_set_thread_area    243
# define __NR_exit_group         252
# define __NR_clock_gettime      265
# define __NR_clock_getres       266

#define SYS_SOCKETPAIR 8
#define SYS_SHUTDOWN   13
#define SYS_SENDMSG    16
#define SYS_RECVMSG    17

#elif defined(__arm__)

# define __NR_exit               1
# define __NR_read               3
# define __NR_write              4
# define __NR_open               5
# define __NR_close              6
# define __NR_link               9
# define __NR_unlink             10
# define __NR_chdir              12
# define __NR_chmod              15
# define __NR_getpid             20
# define __NR_access             33
# define __NR_rename             38
# define __NR_mkdir              39
# define __NR_rmdir              40
# define __NR_dup                41
# define __NR_pipe               42
# define __NR_ioctl              54
# define __NR_dup2               63
# define __NR_gettimeofday       78
# define __NR_symlink            83
# define __NR_readlink           85
# define __NR_munmap             91
# define __NR_wait4              114
# define __NR_clone              120
# define __NR_mprotect           125
# define __NR__llseek            140
# define __NR_sched_yield        158
# define __NR_nanosleep          162
# define __NR_poll               168
# define __NR_prctl              172
# define __NR_rt_sigaction       174
# define __NR_pread64            180
# define __NR_pwrite64           181
# define __NR_getcwd             183
# define __NR_mmap2              192
# define __NR_truncate64         193
# define __NR_stat64             195
# define __NR_lstat64            196
# define __NR_fstat64            197
# define __NR_fcntl64            221
# define __NR_futex              240
# define __NR_exit_group         248
# define __NR_clock_gettime      263
# define __NR_clock_getres       264
# define __NR_socketpair         288
# define __NR_shutdown           293
# define __NR_sendmsg            296
# define __NR_recvmsg            297
# define __NR_ARM_set_tls        0xf0005

#else
# error Unsupported architecture
#endif

#endif
