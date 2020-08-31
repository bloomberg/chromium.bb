// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_PLATFORM_HANDLE_UTILS_H_
#define MOJO_CORE_PLATFORM_HANDLE_UTILS_H_

#include "base/memory/platform_shared_memory_region.h"
#include "mojo/core/system_impl_export.h"
#include "mojo/public/c/system/platform_handle.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace mojo {
namespace core {

// Converts a base shared memory platform handle into one (maybe two on POSIX)
// PlatformHandle(s).
MOJO_SYSTEM_IMPL_EXPORT void ExtractPlatformHandlesFromSharedMemoryRegionHandle(
    base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle handle,
    PlatformHandle* extracted_handle,
    PlatformHandle* extracted_readonly_handle);

// Converts one (maybe two on POSIX) PlatformHandle(s) to a base shared memory
// platform handle.
MOJO_SYSTEM_IMPL_EXPORT
base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle
CreateSharedMemoryRegionHandleFromPlatformHandles(
    PlatformHandle handle,
    PlatformHandle readonly_handle);

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_PLATFORM_HANDLE_UTILS_H_
