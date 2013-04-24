// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/public/common/unicode/putil.h"
#include "third_party/icu/public/common/unicode/udata.h"

#define ICU_UTIL_DATA_SHARED 1
#define ICU_UTIL_DATA_STATIC 2

#ifndef ICU_UTIL_DATA_IMPL

#if defined(OS_WIN)
#define ICU_UTIL_DATA_IMPL ICU_UTIL_DATA_SHARED
#elif defined(OS_MACOSX)
#define ICU_UTIL_DATA_IMPL ICU_UTIL_DATA_STATIC
#elif defined(OS_LINUX)
#define ICU_UTIL_DATA_IMPL ICU_UTIL_DATA_FILE
#endif

#endif  // ICU_UTIL_DATA_IMPL

#if defined(OS_WIN)
#define ICU_UTIL_DATA_SYMBOL "icudt" U_ICU_VERSION_SHORT "_dat"
#define ICU_UTIL_DATA_SHARED_MODULE_NAME "icudt" U_ICU_VERSION_SHORT ".dll"
#endif

bool InitializeICU() {
#if (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_SHARED)
  // We expect to find the ICU data module alongside the current module.
  // Because the module name is ASCII-only, "A" API should be safe.
  // Chrome's copy of ICU dropped a version number XX from icudt dll,
  // but 3rd-party embedders may need it. So, we try both.
  HMODULE module = LoadLibraryA("icudt.dll");
  if (!module) {
    module = LoadLibraryA(ICU_UTIL_DATA_SHARED_MODULE_NAME);
    if (!module)
      return false;
  }

  FARPROC addr = GetProcAddress(module, ICU_UTIL_DATA_SYMBOL);
  if (!addr)
    return false;

  UErrorCode err = U_ZERO_ERROR;
  udata_setCommonData(reinterpret_cast<void*>(addr), &err);
  return err == U_ZERO_ERROR;
#elif (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_STATIC)
  // Mac bundles the ICU data in.
  return true;
#elif (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)
  // We expect to find the ICU data module alongside the current module.
  u_setDataDirectory(".");
  // Only look for the packaged data file;
  // the default behavior is to look for individual files.
  UErrorCode err = U_ZERO_ERROR;
  udata_setFileAccess(UDATA_ONLY_PACKAGES, &err);
  return err == U_ZERO_ERROR;
#endif
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  InitializeICU();

  return RUN_ALL_TESTS();
}
