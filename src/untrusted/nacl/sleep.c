/*
 * Copyright 2009 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/nacl_syscalls.h>

unsigned int sleep(unsigned int seconds) {
  struct timespec req;
  struct timespec rem;

  req.tv_sec = seconds;
  req.tv_nsec = 0;

  if (-1 == nanosleep(&req, &rem)) {
    return rem.tv_sec;
  }
  return 0;
}
