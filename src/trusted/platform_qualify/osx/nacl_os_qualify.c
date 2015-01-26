/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>
#include <sys/utsname.h>

#include "native_client/src/include/build_config.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/platform_qualify/kernel_version.h"
#include "native_client/src/trusted/platform_qualify/nacl_os_qualify.h"


#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32

/*
 * The 64-bit Mac kernel bug was fixed in kernel version 10.8.0, which
 * is in Mac OS X 10.6.8.
 */
static const char *kMinimumKernelVersion = "10.8";

static int KernelVersionIsBuggy(const char *version) {
  int res;
  if (!NaClCompareKernelVersions(version, kMinimumKernelVersion, &res)) {
    NaClLog(LOG_ERROR, "KernelVersionIsBuggy: "
            "Couldn't parse kernel version: %s\n", version);
    return 1;
  }
  return res < 0;
}

#endif

/*
 * Returns 1 if the operating system can run Native Client modules.
 */
int NaClOsIsSupported(void) {
#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32
  struct utsname info;
  if (uname(&info) != 0) {
    NaClLog(LOG_ERROR, "NaClOsIsSupported: uname() failed\n");
    return 0;
  }
  if (strcmp(info.machine, "x86_64") == 0 &&
      KernelVersionIsBuggy(info.release)) {
    NaClLog(LOG_ERROR,
            "NaClOsIsSupported: "
            "This system is running an old 64-bit Mac OS X kernel.  "
            "This kernel version is buggy and can crash when running "
            "Native Client's x86-32 sandbox.  "
            "The fix is to upgrade to Mac OS X 10.6.8 or later, or, as a "
            "workaround, to switch to using a 32-bit kernel, which will "
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
int NaClOsIs64BitWindows(void) {
  return 0;
}
