// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "ppapi/shared_impl/platform_file.h"

namespace ppapi {

// TODO(piman/brettw): Change trusted interface to return a PP_FileHandle,
// those casts are ugly.
base::PlatformFile IntToPlatformFile(int32_t handle) {
#if defined(OS_WIN)
  return reinterpret_cast<HANDLE>(static_cast<intptr_t>(handle));
#elif defined(OS_POSIX)
  return handle;
#else
#error Not implemented.
#endif
}

int32_t PlatformFileToInt(base::PlatformFile handle) {
#if defined(OS_WIN)
  return static_cast<int32_t>(reinterpret_cast<intptr_t>(handle));
#elif defined(OS_POSIX)
  return handle;
#else
#error Not implemented.
#endif
}

}  // namespace ppapi
