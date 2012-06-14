// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/util.h"

namespace playground2 {

bool Util::sendFds(int transport, const void *buf, size_t len, ...) {
  int count = 0;
  va_list ap;
  va_start(ap, len);
  while (va_arg(ap, int) >= 0) {
    ++count;
  }
  va_end(ap);
  if (!count) {
    return false;
  }
  char cmsg_buf[CMSG_SPACE(count*sizeof(int))];
  memset(cmsg_buf, 0, sizeof(cmsg_buf));
  struct iovec  iov[2] = { { 0 } };
  struct msghdr msg    = { 0 };
  int dummy            = 0;
  iov[0].iov_base      = &dummy;
  iov[0].iov_len       = sizeof(dummy);
  if (buf && len > 0) {
    iov[1].iov_base    = const_cast<void *>(buf);
    iov[1].iov_len     = len;
  }
  msg.msg_iov          = iov;
  msg.msg_iovlen       = (buf && len > 0) ? 2 : 1;
  msg.msg_control      = cmsg_buf;
  msg.msg_controllen   = CMSG_LEN(count*sizeof(int));
  struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level     = SOL_SOCKET;
  cmsg->cmsg_type      = SCM_RIGHTS;
  cmsg->cmsg_len       = CMSG_LEN(count*sizeof(int));
  va_start(ap, len);
  for (int i = 0, fd; (fd = va_arg(ap, int)) >= 0; ++i) {
    (reinterpret_cast<int *>(CMSG_DATA(cmsg)))[i] = fd;
  }
  return sendmsg(transport, &msg, 0) ==
      static_cast<ssize_t>(sizeof(dummy) + ((buf && len > 0) ? len : 0));
}

bool Util::getFds(int transport, void *buf, size_t *len, ...) {
  int count = 0;
  va_list ap;
  va_start(ap, len);
  for (int *fd; (fd = va_arg(ap, int *)) != NULL; ++count) {
    *fd = -1;
  }
  va_end(ap);
  if (!count) {
    return false;
  }
  char cmsg_buf[CMSG_SPACE(count*sizeof(int))];
  memset(cmsg_buf, 0, sizeof(cmsg_buf));
  struct iovec iov[2] = { { 0 } };
  struct msghdr msg   = { 0 };
  int err;
  iov[0].iov_base     = &err;
  iov[0].iov_len      = sizeof(int);
  if (buf && len && *len > 0) {
    iov[1].iov_base   = buf;
    iov[1].iov_len    = *len;
  }
  msg.msg_iov         = iov;
  msg.msg_iovlen      = (buf && len && *len > 0) ? 2 : 1;
  msg.msg_control     = cmsg_buf;
  msg.msg_controllen  = CMSG_LEN(count*sizeof(int));
  ssize_t bytes = recvmsg(transport, &msg, 0);
  if (len) {
    *len = bytes > static_cast<int>(sizeof(int)) ? bytes - sizeof(int) : 0;
  }
  if (bytes != static_cast<ssize_t>(sizeof(int) + iov[1].iov_len)) {
    if (bytes >= 0) {
      errno = 0;
    }
    return false;
  }
  if (err) {
    // "err" is the first four bytes of the payload. If these are non-zero,
    // the sender on the other side of the socketpair sent us an errno value.
    // We don't expect to get any file handles in this case.
    errno = err;
    return false;
  }
  struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
  if ((msg.msg_flags & (MSG_TRUNC|MSG_CTRUNC)) ||
      !cmsg                                    ||
      cmsg->cmsg_level != SOL_SOCKET           ||
      cmsg->cmsg_type  != SCM_RIGHTS           ||
      cmsg->cmsg_len   != CMSG_LEN(count*sizeof(int))) {
    errno = EBADF;
    return false;
  }
  va_start(ap, len);
  for (int *fd, i = 0; (fd = va_arg(ap, int *)) != NULL; ++i) {
    *fd = (reinterpret_cast<int *>(CMSG_DATA(cmsg)))[i];
  }
  va_end(ap);
  return true;
}

void Util::closeAllBut(int fd, ...) {
  int proc_fd;
  int fdir;
  if ((proc_fd = Sandbox::getProcFd()) < 0 ||
      (fdir = openat(proc_fd, "self/fd", O_RDONLY|O_DIRECTORY)) < 0) {
    Sandbox::die("Cannot access \"/proc/self/fd\"");
  }
  int dev_null = open("/dev/null", O_RDWR);
  DIR *dir = fdopendir(fdir);
  struct dirent de, *res;
  while (!readdir_r(dir, &de, &res) && res) {
    if (res->d_name[0] < '0') {
      continue;
    }
    int i = atoi(res->d_name);
    if (i >= 0 && i != dirfd(dir) && i != dev_null) {
      va_list ap;
      va_start(ap, fd);
      for (int f = fd;; f = va_arg(ap, int)) {
        if (f < 0) {
          if (i <= 2) {
            // Never ever close 0..2. If we cannot redirect to /dev/null,
            // then we are better off leaving the standard descriptors open.
            if (dev_null >= 0) {
              if (HANDLE_EINTR(dup2(dev_null, i))) {
                Sandbox::die("Cannot dup2()");
              }
            }
          } else {
            if (HANDLE_EINTR(close(i))) { }
          }
          break;
        } else if (i == f) {
          break;
        }
      }
      va_end(ap);
    }
  }
  closedir(dir);
  if (dev_null >= 0) {
    if (HANDLE_EINTR(close(dev_null))) { }
  }
  return;
}

}  // namespace
