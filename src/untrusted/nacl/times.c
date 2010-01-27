/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routine for times for newlib support.
 */

#include <sys/times.h>
#include <errno.h>

clock_t times(struct tms *buf) {
  /* NOTE(sehr): NOT IMPLEMENTED: times */
  errno = EACCES;
  return -1;
}
