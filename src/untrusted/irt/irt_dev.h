/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_DEV_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_DEV_H_

#include <stdarg.h>
#include <stddef.h>
#include <sys/types.h>

/* Use relative path so that irt_dev.h can be installed as a system header. */
#include "irt.h"

/*
 * This header file is for IRT interfaces that are only available in
 * Chromium behind a flag, or are not available in Chromium at all.
 *
 * For example, the filesystem interfaces and getpid() are only
 * available when the env var NACL_DANGEROUS_ENABLE_FILE_ACCESS is
 * set, which enables an unsafe debugging mode.
 */

struct dirent;
struct stat;
struct timeval;

struct NaClMemMappingInfo;

#if defined(__cplusplus)
extern "C" {
#endif

#define NACL_IRT_DEV_FDIO_v0_2  "nacl-irt-dev-fdio-0.2"
struct nacl_irt_dev_fdio_v0_2 {
  int (*close)(int fd);
  int (*dup)(int fd, int *newfd);
  int (*dup2)(int fd, int newfd);
  int (*read)(int fd, void *buf, size_t count, size_t *nread);
  int (*write)(int fd, const void *buf, size_t count, size_t *nwrote);
  int (*seek)(int fd, nacl_irt_off_t offset, int whence,
              nacl_irt_off_t *new_offset);
  int (*fstat)(int fd, struct stat *);
  int (*getdents)(int fd, struct dirent *, size_t count, size_t *nread);
  int (*fchdir)(int fd);
  int (*fchmod)(int fd, mode_t mode);
  int (*fsync)(int fd);
  int (*fdatasync)(int fd);
  int (*ftruncate)(int fd, nacl_irt_off_t length);
};

#define NACL_IRT_DEV_FDIO_v0_3  "nacl-irt-dev-fdio-0.3"
struct nacl_irt_dev_fdio {
  int (*close)(int fd);
  int (*dup)(int fd, int *newfd);
  int (*dup2)(int fd, int newfd);
  int (*read)(int fd, void *buf, size_t count, size_t *nread);
  int (*write)(int fd, const void *buf, size_t count, size_t *nwrote);
  int (*seek)(int fd, nacl_irt_off_t offset, int whence,
              nacl_irt_off_t *new_offset);
  int (*fstat)(int fd, struct stat *);
  int (*getdents)(int fd, struct dirent *, size_t count, size_t *nread);
  int (*fchdir)(int fd);
  int (*fchmod)(int fd, mode_t mode);
  int (*fsync)(int fd);
  int (*fdatasync)(int fd);
  int (*ftruncate)(int fd, nacl_irt_off_t length);
  int (*isatty)(int fd, int *result);
};

/*
 * The "irt-dev-filename" is similiar to "irt-filename" but provides
 * additional functions, including those that do directory manipulation.
 * Inside Chromium, requests for this interface may fail, or may return
 * functions which always return errors.
 */
#define NACL_IRT_DEV_FILENAME_v0_2 "nacl-irt-dev-filename-0.2"
struct nacl_irt_dev_filename_v0_2 {
  int (*open)(const char *pathname, int oflag, mode_t cmode, int *newfd);
  int (*stat)(const char *pathname, struct stat *);
  int (*mkdir)(const char *pathname, mode_t mode);
  int (*rmdir)(const char *pathname);
  int (*chdir)(const char *pathname);
  int (*getcwd)(char *pathname, size_t len);
  int (*unlink)(const char *pathname);
};

#define NACL_IRT_DEV_FILENAME_v0_3 "nacl-irt-dev-filename-0.3"
struct nacl_irt_dev_filename {
  int (*open)(const char *pathname, int oflag, mode_t cmode, int *newfd);
  int (*stat)(const char *pathname, struct stat *);
  int (*mkdir)(const char *pathname, mode_t mode);
  int (*rmdir)(const char *pathname);
  int (*chdir)(const char *pathname);
  int (*getcwd)(char *pathname, size_t len);
  int (*unlink)(const char *pathname);
  int (*truncate)(const char *pathname, nacl_irt_off_t length);
  int (*lstat)(const char *pathname, struct stat *);
  int (*link)(const char *oldpath, const char *newpath);
  int (*rename)(const char *oldpath, const char *newpath);
  int (*symlink)(const char *oldpath, const char *newpath);
  int (*chmod)(const char *path, mode_t mode);
  int (*access)(const char *path, int amode);
  int (*readlink)(const char *path, char *buf, size_t count, size_t *nread);
  int (*utimes)(const char *filename, const struct timeval *times);
};

#define NACL_IRT_DEV_LIST_MAPPINGS_v0_1 \
  "nacl-irt-dev-list-mappings-0.1"
struct nacl_irt_dev_list_mappings {
  int (*list_mappings)(struct NaClMemMappingInfo *regions,
                       size_t count, size_t *result_count);
};

#define NACL_IRT_DEV_GETPID_v0_1 "nacl-irt-dev-getpid-0.1"
struct nacl_irt_dev_getpid {
  int (*getpid)(int *pid);
};

/*
 * This is an internal interface, for use by PNaCl's sandboxed linker for
 * communicating with the browser.
 *
 * serve_link_request(func) waits for a request via IPC, and then calls
 * func(), on the same thread.
 *
 * |nexe_fd| is a writable FD for the linker to write its output to.
 * |obj_file_fds| is an array of readable FDs (of length
 * |obj_file_fd_count|) of input object files.  func() returns zero on
 * success or a non-zero value on error.
 */
#define NACL_IRT_PRIVATE_PNACL_TRANSLATOR_LINK_v0_1 \
    "nacl-irt-private-pnacl-translator-link-0.1"
struct nacl_irt_private_pnacl_translator_link {
  void (*serve_link_request)(int (*func)(int nexe_fd,
                                         const int *obj_file_fds,
                                         int obj_file_fd_count));
};

#if defined(__cplusplus)
}
#endif

#endif /* NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_DEV_H */
