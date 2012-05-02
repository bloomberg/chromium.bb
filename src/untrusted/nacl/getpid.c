/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


/*
 * Stub routine for wait for newlib support.
 */
#include <errno.h>
#include <sys/types.h>
#include <sys/nacl_syscalls.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

pid_t getpid(void) {
  /*
   * Note that getpid() is not normally documented as returning an
   * error or setting errno.  For example,
   * http://pubs.opengroup.org/onlinepubs/007904975/functions/getpid.html
   * says "The getpid() function shall always be successful and no
   * return value is reserved to indicate an error".  However, that is
   * not reasonable for systems that don't support getpid(), such as
   * NaCl in the web browser.
   *
   * TODO(mseaborn): Create an IRT call for getpid().
   * See http://code.google.com/p/nativeclient/issues/detail?id=2488
   */
  int retval = NACL_SYSCALL(getpid)();
  if (retval < 0) {
    errno = -retval;
    return -1;
  }
  return retval;
}
