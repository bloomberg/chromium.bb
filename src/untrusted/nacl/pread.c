/*
 * Copyright 2016 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <unistd.h>

#include "native_client/src/untrusted/nacl/nacl_irt.h"

ssize_t pread(int desc, void *buf, size_t count, off_t offset) {
  if (!__libnacl_irt_init_fn(&__libnacl_irt_dev_fdio.pread,
                             __libnacl_irt_dev_fdio_init)) {
    return -1;
  }

  size_t nread;
  int error = __libnacl_irt_dev_fdio.pread(desc, buf, count, offset, &nread);
  if (error) {
    errno = error;
    return -1;
  }
  return nread;
}
