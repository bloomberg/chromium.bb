/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_H
#define NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_H

#if __cplusplus
extern "C" {
#endif

#include <sys/time.h>  /* for gettimeofday() */
#include <sys/types.h>
#include <sys/stat.h>  /* for stat */
#include <time.h>      /* for clock() */

/* ============================================================ */
/* files */
/* ============================================================ */

typedef int (*TYPE_irt_dup)(int oldfd);

typedef int (*TYPE_irt_dup2)(int oldfd, int newfd);

typedef int (*TYPE_irt_read) (int desc, void *buf, size_t count);

typedef int (*TYPE_irt_close) (int desc);

typedef int (*TYPE_irt_fstat) (int fd, struct stat *stbp);

typedef int (*TYPE_irt_write) (int desc, void const *buf, size_t count);

typedef int (*TYPE_irt_open) (const char *pathname, int oflag, ...);

typedef off_t (*TYPE_irt_lseek) (int desc,
                                 off_t offset, /* 64 bit value */
                                 int whence);

typedef int (*TYPE_irt_stat) (const char *file, struct stat *st);

/* ============================================================ */
/* mmap */
/* ============================================================ */

/* TODO(bradchen) figure out of offset needs to be a 64-bit off_t * */
typedef void *(*TYPE_irt_mmap) (void *start,
                                size_t length,
                                int prot,
                                int flags,
                                int desc,
                                off_t offset);

typedef int (*TYPE_irt_munmap) (void *start, size_t length);

/* ============================================================ */
/* misc */
/* ============================================================ */

typedef int (*TYPE_irt_gettimeofday) (struct timeval *tv, void *tz);

typedef void *(*TYPE_irt_sysbrk) (void *p);

typedef clock_t (*TYPE_irt_clock) (void);

typedef int (*TYPE_irt_nanosleep) (const struct timespec *req,
                                   struct timespec *rem);

#ifdef __GNUC__
typedef void (*TYPE_irt_exit) (int status) __attribute__((noreturn));
#else
typedef void (*TYPE_irt_exit) (int status);
#endif

typedef int (*TYPE_irt_dyncode_create) (void *dest, const void *src,
                                       size_t size);

typedef int (*TYPE_irt_dyncode_modify) (void *dest, const void *src,
                                       size_t size);

typedef int (*TYPE_irt_dyncode_delete) (void *dest, size_t size);

typedef const char *(*TYPE_irt_version)(int *major, int *minor);

/* This must either be static or be a #define to avoid */
/* multiply-defined symbols */
#define NACLIRTv0_1 "Native Client Core v0.1"
struct nacl_core {
  TYPE_irt_version nacl_core_version;
  /* POSIX-like */
  TYPE_irt_clock nacl_core_clock;
  TYPE_irt_exit nacl_core_exit;
  TYPE_irt_gettimeofday nacl_core_gettimeofday;
  TYPE_irt_nanosleep nacl_core_nanosleep;
  TYPE_irt_sysbrk nacl_core_sysbrk;
  /* POSIX-file and memory operations */
  TYPE_irt_close nacl_core_close;
  TYPE_irt_dup nacl_core_dup;
  TYPE_irt_dup2 nacl_core_dup2;
  TYPE_irt_fstat nacl_core_fstat;
  TYPE_irt_lseek nacl_core_lseek;
  TYPE_irt_mmap nacl_core_mmap;
  TYPE_irt_munmap nacl_core_munmap;
  TYPE_irt_open nacl_core_open;
  TYPE_irt_read nacl_core_read;
  TYPE_irt_stat nacl_core_stat;
  TYPE_irt_write nacl_core_write;
  /* NaCl-specific */
  TYPE_irt_dyncode_create nacl_core_dyncode_create;
  TYPE_irt_dyncode_delete nacl_core_dyncode_delete;
  TYPE_irt_dyncode_modify nacl_core_dyncode_modify;
};

const void *nacl_getinterface(const char *version);

#if __cplusplus
}
#endif

#endif /* NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_H */
