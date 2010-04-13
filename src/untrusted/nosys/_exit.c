/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routine for `_exit' for application porting support.
 */

#include <limits.h>
#include <stdlib.h>
#include <errno.h>

void _exit(int status) {
  /* Kill the process in a bad way */
  int x = status / INT_MAX;
  if (x > 0) _exit(status);
  _exit(42 / status);
}
