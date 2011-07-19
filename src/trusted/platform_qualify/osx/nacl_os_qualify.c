/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>
#include <sys/utsname.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/platform_qualify/nacl_os_qualify.h"


#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32

static int ParseKernelVersion(const char *version,
                              int *number1,
                              int *number2) {
  char *next;
  *number1 = strtol(version, &next, 10);
  if (next == NULL || *next != '.') {
    return 0;
  }
  *number2 = strtol(next + 1, &next, 10);
  if (next == NULL || *next != '.') {
    return 0;
  }
  /* Ignore the rest of the version string. */
  return 1;
}

/*
 * The 64-bit Mac kernel bug was fixed in kernel version 10.8.0, which
 * is in Mac OS X 10.6.8.
 */
static const int kMinimumKernelMajorVersion = 10;
static const int kMinimumKernelMinorVersion = 8;

static int KernelVersionIsBuggy(const char *version) {
  int number1;
  int number2;
  return !ParseKernelVersion(version, &number1, &number2)
         || number1 < kMinimumKernelMajorVersion
         || (number1 == kMinimumKernelMajorVersion
             && number2 < kMinimumKernelMinorVersion);
}

#endif

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
