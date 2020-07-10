/*
 * Copyright 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Stub routine for `bind' for porting support.
 */

#include <errno.h>

struct sockaddr;

int bind(int sockfd, const struct sockaddr *addr, unsigned int addrlen) {
  errno = ENOSYS;
  return -1;
}
