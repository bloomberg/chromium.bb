// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gpu_memory_buffer.h"

namespace gfx {

GpuMemoryBufferHandle::GpuMemoryBufferHandle()
    : type(EMPTY_BUFFER), id(0), handle(base::SharedMemory::NULLHandle()) {
}

GpuMemoryBufferHandle::GpuMemoryBufferHandle(
    const GpuMemoryBufferHandle& other) = default;

GpuMemoryBufferHandle::~GpuMemoryBufferHandle() {}

void GpuMemoryBuffer::SetColorSpaceForScanout(
    const gfx::ColorSpace& color_space) {}

GpuMemoryBufferHandle CloneHandleForIPC(
    const GpuMemoryBufferHandle& source_handle) {
  switch (source_handle.type) {
    case gfx::EMPTY_BUFFER:
      NOTREACHED();
      return source_handle;
    case gfx::SHARED_MEMORY_BUFFER: {
      gfx::GpuMemoryBufferHandle handle;
      handle.type = gfx::SHARED_MEMORY_BUFFER;
      handle.handle = base::SharedMemory::DuplicateHandle(source_handle.handle);
      handle.offset = source_handle.offset;
      handle.stride = source_handle.stride;
      return handle;
    }
    case gfx::OZONE_NATIVE_PIXMAP: {
      gfx::GpuMemoryBufferHandle handle;
      handle.type = gfx::OZONE_NATIVE_PIXMAP;
      handle.id = source_handle.id;
#if defined(USE_OZONE)
      handle.native_pixmap_handle =
          gfx::CloneHandleForIPC(source_handle.native_pixmap_handle);
#endif
      return handle;
    }
    case gfx::IO_SURFACE_BUFFER:
      return source_handle;
  }
  return gfx::GpuMemoryBufferHandle();
}

}  // namespace gfx
