/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <unistd.h>

#include "native_client/src/untrusted/nacl/nacl_irt.h"

char *getcwd(char *buf, size_t size) {
  if (__libnacl_irt_dev_filename.getcwd == NULL) {
    __libnacl_irt_filename_init();
    if (__libnacl_irt_dev_filename.getcwd == NULL) {
      errno = ENOSYS;
      return NULL;
    }
  }

  int error = __libnacl_irt_dev_filename.getcwd(buf, size);
  if (error) {
    errno = error;
    return NULL;
  }

  return buf;
}
