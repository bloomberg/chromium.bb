// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/nexe_arch.h"
#include "native_client/src/trusted/platform_qualify/nacl_os_qualify.h"

namespace {
// The list of supported ISA strings for x86.  See issue:
//   http://code.google.com/p/nativeclient/issues/detail?id=1040 for more
// information.  Note that these string are to be case-insensitive compared.
const char* const kNexeArchX86_32 = "x86-32";
const char* const kNexeArchX86_64 = "x86-64";
}  // namespace

namespace plugin {
const char* GetSandboxISA() {
#if (NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64) && \
    (defined(NACL_LINUX) || defined(NACL_OSX))
  return kNexeArchX86_64;  // 64-bit Linux or Mac.
#else
  return NaClOsIs64BitWindows() == 1
      ? kNexeArchX86_64  // 64-bit Windows (Chrome, Firefox)
      : kNexeArchX86_32;  // everything else.
#endif
}
}  // namespace plugin
