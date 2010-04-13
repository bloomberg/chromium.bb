/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>

#include <sys/nacl_syscalls.h>


int main() {
  /* TODO(mseaborn): Implement the syscall! */
  uint8_t buf[32];
  int rc = nacl_dyncode_copy(buf, buf, sizeof(buf));
  assert(rc == -1);
  assert(errno == ENOSYS);
  return 0;
}
