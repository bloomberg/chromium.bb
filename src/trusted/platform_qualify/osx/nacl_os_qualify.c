/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>
#include <sys/utsname.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/platform_qualify/nacl_os_qualify.h"

/*
 * Returns 1 if the operating system can run Native Client modules.
 */
int NaClOsIsSupported() {
#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32
  struct utsname info;
  if (uname(&info) != 0) {
    NaClLog(LOG_ERROR, "NaClOsIsSupported: uname() failed\n");
    return 0;
  }
  if (strcmp(info.machine, "x86_64") == 0) {
    /*
     * TODO(mseaborn): We believe the kernel bug is fixed in Mac OS X
     * 10.7 (Lion).  We could check for the new kernel version or OS
     * version and enable NaCl on new systems.
     */
    NaClLog(LOG_ERROR,
            "NaClOsIsSupported: "
            "This system is running a 64-bit Mac OS X kernel.  "
            "64-bit Mac kernels are buggy and cannot handle "
            "Native Client's x86-32 sandbox.  "
            "A workaround is to switch to using a 32-bit kernel, which will "
            "be capable of running Native Client.  For more information, see "
            "http://code.google.com/p/nativeclient/issues/detail?id=1712\n");
    return 0;
  }
#endif
  return 1;
}


/*
 * Returns 1 if the operating system is a 64-bit version of
 * Windows.  For now, all of these versions are not supported.
 */
int NaClOsIs64BitWindows() {
  return 0;
}

/*
 * Returns 1 if the operating system is a version that restores the
 * local descriptor table of processes.  This determines which 64-bit
 * solution is required to execute Native Client modules.
 */
int NaClOsRestoresLdt() {
  return 1;
}
