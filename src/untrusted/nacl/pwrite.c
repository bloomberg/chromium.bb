/*
 * Copyright 2016 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <unistd.h>

#include "native_client/src/untrusted/nacl/nacl_irt.h"

ssize_t pwrite(int desc, const void *buf, size_t count, off_t offset) {
  if (!__libnacl_irt_init_fn(&__libnacl_irt_dev_fdio.pwrite,
                             __libnacl_irt_dev_fdio_init)) {
    return -1;
  }

  size_t nwrote;
  int error = __libnacl_irt_dev_fdio.pwrite(desc, buf, count, offset, &nwrote);
  if (error) {
    errno = error;
    return -1;
  }
  return nwrote;
}
