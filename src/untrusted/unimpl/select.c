/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routines for select/pselect for porting support.
 */

/* #include <sys/select.h> */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

int select(int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
           struct timeval *timeout) {
  /* NOTE(sehr): NOT IMPLEMENTED: select */
  errno = ENOMEM;
  return -1;
}

typedef unsigned int sigset_t;

int pselect(int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
           const struct timespec *timeout, const sigset_t *sigmask) {
  /* NOTE(sehr): NOT IMPLEMENTED: pselect */
  errno = ENOMEM;
  return -1;
}
