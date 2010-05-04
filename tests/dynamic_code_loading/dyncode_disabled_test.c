/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>

#include <sys/nacl_syscalls.h>


/* TODO(mseaborn): Add a symbol to the linker script for finding the
   end of the static code segment more accurately.  The value below is
   an approximation. */
#define DYNAMIC_CODE_SEGMENT_START 0x80000


/* This test checks that it is being run in the context of dynamic
   loading being disabled. */

int main() {
  void *dest = (void *) DYNAMIC_CODE_SEGMENT_START;
  char buf[1];
  int rc = nacl_dyncode_copy(dest, buf, 0);
  assert(rc == -1);
  assert(errno == EINVAL);
  return 0;
}
