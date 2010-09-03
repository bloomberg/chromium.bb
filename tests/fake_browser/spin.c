/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <assert.h>
#include <sys/nacl_syscalls.h>
#include <unistd.h>


int main() {
  /* Accept the plugin's connection so that the plugin does not block
     forever. */
  int fd;
  int rc;
  fd = imc_accept(3);
  assert(fd >= 0);
  rc = close(fd);
  assert(rc == 0);

  /* Run forever.  This is useful for testing that the plugin can shut
     down this process OK.  If this process is left behind by the
     plugin, it will be more obvious if it is consuming CPU than if it
     is sleeping! */
  while (1) { }

  return 0;
}
