/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <fcntl.h>

#include "native_client/src/public/name_service.h"
#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/irt/irt_private.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

static int secure_random_fd = -1;

static int prepare_secure_random(void) {
  if (secure_random_fd == -1) {
    int fd;
    int status = irt_nameservice_lookup("SecureRandom", O_RDONLY, &fd);
    if (status != NACL_NAME_SERVICE_SUCCESS) {
      return EIO;
    }
    if (!__sync_bool_compare_and_swap(&secure_random_fd, -1, fd)) {
      /*
       * We raced with another thread.  It already installed an fd.
       */
      NACL_SYSCALL(close)(fd);
    }
  }
  return 0;
}

static int nacl_irt_get_random_bytes(void *buf, size_t count, size_t *nread) {
  int error = prepare_secure_random();
  if (error == 0) {
    int rv = NACL_GC_WRAP_SYSCALL(NACL_SYSCALL(read)(secure_random_fd,
                                                     buf, count));
    if (rv < 0)
      error = -rv;
    else
      *nread = rv;
  }
  return error;
}

const struct nacl_irt_random nacl_irt_random = {
  nacl_irt_get_random_bytes,
};
