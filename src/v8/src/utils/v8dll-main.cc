// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The GYP based build ends up defining USING_V8_SHARED when compiling this
// file.
#undef USING_V8_SHARED
#include "include/v8config.h"
#include "base/allocator/buildflags.h"
#include "build/build_config.h"

#if V8_OS_WIN
#include <locale.h>
#include <windows.h>
#include "src/base/win32-headers.h"

extern "C" {
BOOL WINAPI DllMain(HANDLE hinstDLL, DWORD dwReason, LPVOID lpvReserved) {
  if (dwReason == DLL_PROCESS_ATTACH) {
    setlocale(LC_ALL, NULL);
  }

  // Do nothing.
  return 1;
}
}
#endif  // V8_OS_WIN
