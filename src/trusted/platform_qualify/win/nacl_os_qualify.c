/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/platform_qualify/nacl_os_qualify.h"

/*
 * Returns 1 if the operating system can run Native Client modules.
 */
int NaClOsIsSupported() {
  SYSTEM_INFO system_info;
  OSVERSIONINFO version_info;

  memset(&version_info, 0, sizeof(version_info));
  version_info.dwOSVersionInfoSize = sizeof(version_info);
  if (!GetVersionEx(&version_info)) {
    /* If the API doesn't work we have to assume the worst. */
    return 0;
  }
  if (5 > version_info.dwMajorVersion ||
      ((5 == version_info.dwMajorVersion) &&
       (0 == version_info.dwMinorVersion))) {
    /* We do not support versions prior to Windows XP. */
    return 0;
  }
  /*
   * GetNativeSystemInfo is only available on Windows XP and after.
   */
  GetNativeSystemInfo(&system_info);
  if (PROCESSOR_ARCHITECTURE_AMD64 != system_info.wProcessorArchitecture &&
      PROCESSOR_ARCHITECTURE_INTEL != system_info.wProcessorArchitecture) {
    /*
     * The installed operating system processor architecture is either
     * Itanium or unknown.
     */
    return 0;
  }
  if (NaClOsIs64BitWindows()) {
    /* For now, all 64-bit Windows versions are not supported. */
    return 0;
  }
  return 1;
}


/*
 * Returns 1 if the operating system is a 64-bit version of
 * Windows.  For now, all of these versions are not supported.
 */
int NaClOsIs64BitWindows() {
  SYSTEM_INFO system_info;

  GetNativeSystemInfo(&system_info);
  if (PROCESSOR_ARCHITECTURE_AMD64 == system_info.wProcessorArchitecture) {
    /*
     * The installed operating system processor architecture is x86-64.
     * This assumes the caller already knows it's a supported architecture.
     */
    return 1;
  }
  return 0;
}

/*
 * Returns 1 if the operating system is a version that restores the
 * local descriptor table of processes.  This determines which 64-bit
 * solution is required to execute Native Client modules.
 */
int NaClOsRestoresLdt() {
  OSVERSIONINFO version_info;

  if (!NaClOsIs64BitWindows()) {
    /*
     * All known 32-bit versions of Windows restore the LDT after context
     * switches.
     */
    return 1;
  }
  memset(&version_info, 0, sizeof(version_info));
  version_info.dwOSVersionInfoSize = sizeof(version_info);
  if (!GetVersionEx(&version_info)) {
    /* If the API doesn't work we have to assume the worst. */
    return 0;
  }

  if (6 == version_info.dwMajorVersion &&
      1 == version_info.dwMinorVersion) {
    /*
     * Windows 7 is believed to restore the LDT on context switches.
     * This code also allows Windows Server 2008 R2.
     * TODO(sehr): Need to confirm Windows Server 2008 R2.
     */
    return 1;
  }
  /*
   * Every other 64-bit version may have issues restoring the LDT.
   */
  return 0;
}
