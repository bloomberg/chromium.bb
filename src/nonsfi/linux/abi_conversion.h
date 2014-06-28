/*
 * Copyright 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_NONSFI_LINUX_ABI_CONVERSION_H_
#define NATIVE_CLIENT_SRC_NONSFI_LINUX_ABI_CONVERSION_H_ 1

#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include "native_client/src/nonsfi/linux/linux_syscall_defines.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"

#if defined(__i386__) || defined(__arm__)

struct linux_abi_timeval {
  int32_t tv_sec;
  int32_t tv_usec;
};

struct linux_abi_timespec {
  int32_t tv_sec;
  int32_t tv_nsec;
};

struct linux_abi_stat64 {
  uint64_t st_dev;
  uint32_t pad1;
  uint32_t st_ino_32;
  uint32_t st_mode;
  uint32_t st_nlink;
  uint32_t st_uid;
  uint32_t st_gid;
  uint64_t st_rdev;
  uint32_t pad2;
  /*
   * ARM, PNaCl, and other ABIs require a 32bit padding for alignment
   * here, while i386 does not.
   */
#if defined(__arm__)
  uint32_t pad3;
#endif
  int64_t st_size;
  uint32_t st_blksize;
#if defined(__arm__)
  uint32_t pad4;
#endif
  uint64_t st_blocks;
  int32_t st_atime;
  uint32_t st_atime_nsec;
  int32_t st_mtime;
  uint32_t st_mtime_nsec;
  int32_t st_ctime;
  uint32_t st_ctime_nsec;
  uint64_t st_ino;

  /*
   * We control the layout of this struct explicitly because PNaCl
   * clang inserts extra paddings even for the i386 target without
   * this.
   */
} __attribute__((packed));

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

/* Converts the stat64 struct from Linux's to NaCl's ABI. */
static inline void linux_stat_to_nacl_stat(
    const struct linux_abi_stat64 *linux_stat,
    struct stat *nacl_stat) {
  /*
   * Some fields in linux_stat, such as st_dev, group/other bits of mode
   * and (a,m,c)timensec, are ignored to sync with the NaCl's original
   * implementation. Please see also NaClAbiStatHostDescStatXlateCtor
   * in native_client/src/trusted/desc/posix/nacl_desc.c.
   */
  memset(nacl_stat, 0, sizeof(*nacl_stat));
  nacl_stat->st_dev = 0;
  nacl_stat->st_ino = NACL_FAKE_INODE_NUM;
  nacl_stat->st_mode = linux_stat->st_mode;
  nacl_stat->st_nlink = linux_stat->st_nlink;
  nacl_stat->st_uid = -1;  /* Not root */
  nacl_stat->st_gid = -1;  /* Not wheel */
  nacl_stat->st_rdev = 0;
  nacl_stat->st_size = linux_stat->st_size;
  nacl_stat->st_blksize = 0;
  nacl_stat->st_blocks = 0;
  nacl_stat->st_atime = linux_stat->st_atime;
  nacl_stat->st_mtime = linux_stat->st_mtime;
  nacl_stat->st_ctime = linux_stat->st_ctime;
}

#endif
