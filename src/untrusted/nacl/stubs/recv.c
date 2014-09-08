/*
 * Copyright 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


/*
 * Stub routine for `recv' for porting support.
 */

#include <errno.h>
#include <sys/types.h>
/* We would #include <sys/socket.h> if newlib had this header. */

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
  errno = ENOSYS;
  return -1;
}
