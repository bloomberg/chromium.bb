/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Stub routine for various function that are not part of pnacl's newlib
 */

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int remove(const char *pathname) {
  errno = ENOSYS;
  return -1;
}

int rename(const char *oldpath, const char *newpath) {
  errno = ENOSYS;
  return -1;
}

typedef void (*sighandler_t)(int);

sighandler_t signal(int signum, sighandler_t handler) {
  errno = ENOSYS;
  return SIG_ERR;
}

int system(const char *command) {
  errno = ENOSYS;
  return -1;
}

FILE *tmpfile(void) {
  errno = ENOSYS;
  return 0;
}

#include "native_client/src/untrusted/nosys/warning.h"
stub_warning(remove);
stub_warning(rename);
stub_warning(signal);
stub_warning(system);
stub_warning(tmpfile);
