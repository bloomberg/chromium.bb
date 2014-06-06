/*
 * Copyright 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_NONSFI_LINUX_ABI_CONVERSION_H_
#define NATIVE_CLIENT_SRC_NONSFI_LINUX_ABI_CONVERSION_H_ 1

#include <stdint.h>
#include <sys/time.h>
#include <time.h>

#if defined(__i386__) || defined(__arm__)

struct linux_abi_timeval {
  int32_t tv_sec;
  int32_t tv_usec;
};

struct linux_abi_timespec {
  int32_t tv_sec;
  int32_t tv_nsec;
};

#else
# error Unsupported architecture
#endif

/* Converts the timespec struct from NaCl's to Linux's ABI. */
static inline void nacl_timespec_to_linux_timespec(
    const struct timespec *nacl_timespec,
    struct linux_abi_timespec *linux_timespec) {
  linux_timespec->tv_sec = nacl_timespec->tv_sec;
  linux_timespec->tv_nsec = nacl_timespec->tv_nsec;
}

/* Converts the timespec struct from Linux's to NaCl's ABI. */
static inline void linux_timespec_to_nacl_timespec(
    const struct linux_abi_timespec *linux_timespec,
    struct timespec *nacl_timespec) {
  nacl_timespec->tv_sec = linux_timespec->tv_sec;
  nacl_timespec->tv_nsec = linux_timespec->tv_nsec;
}

/* Converts the timeval struct from Linux's to NaCl's ABI. */
static inline void linux_timeval_to_nacl_timeval(
    const struct linux_abi_timeval *linux_timeval,
    struct timeval *nacl_timeval) {
  nacl_timeval->tv_sec = linux_timeval->tv_sec;
  nacl_timeval->tv_usec = linux_timeval->tv_usec;
}

#endif
