// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_BITMAP_ALLOCATION_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_BITMAP_ALLOCATION_H_

#include <memory>

#include "base/memory/read_only_shared_memory_region.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/viz_common_export.h"
#include "mojo/public/cpp/system/buffer.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace viz {

namespace bitmap_allocation {

// Allocates a read-only shared memory region and its writable mapping to hold
// |size| pixels in specific |format|. Crashes if allocation does not succeed.
VIZ_COMMON_EXPORT base::MappedReadOnlyRegion AllocateSharedBitmap(
    const gfx::Size& size,
    ResourceFormat format);

// Converts a base::ReadOnlySharedMemoryRegion to its corresponding
// Mojo scoped handle. This simply calls mojo::WrapReadOnlySharedMemoryRegion()
// but allows the caller to not include the corresponding header where it is
// defined. Moreover, it will be easy to grep for all uses of this method
// in the future when MojoHandles will not longer be necessary.
// TODO(crbug.com/951391): Remove once refactor is completed.
VIZ_COMMON_EXPORT mojo::ScopedSharedBufferHandle ToMojoHandle(
    base::ReadOnlySharedMemoryRegion region);

// Converts a scoped Mojo handle back to a base::ReadOnlySharedMemoryRegion
// This simply calls mojo::UnwrapReadOnlySharedMemoryRegion(), but has the same
// benefits as ToMojoHandle() described above.
// TODO(crbug.com/951391): Remove once refactor is completed.
VIZ_COMMON_EXPORT base::ReadOnlySharedMemoryRegion FromMojoHandle(
    mojo::ScopedSharedBufferHandle handle);

}  // namespace bitmap_allocation

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_BITMAP_ALLOCATION_H_
