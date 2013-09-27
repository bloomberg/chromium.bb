// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GPU_MEMORY_BUFFER_H_
#define UI_GFX_GPU_MEMORY_BUFFER_H_

#include "base/memory/shared_memory.h"
#include "build/build_config.h"
#include "ui/gfx/gfx_export.h"

#if defined(OS_ANDROID)
#include <third_party/khronos/EGL/egl.h>
#endif

namespace gfx {

enum GpuMemoryBufferType {
  EMPTY_BUFFER,
  SHARED_MEMORY_BUFFER,
  EGL_CLIENT_BUFFER
};

struct GpuMemoryBufferHandle {
  GpuMemoryBufferHandle()
      : type(EMPTY_BUFFER),
        handle(base::SharedMemory::NULLHandle())
#if defined(OS_ANDROID)
        , native_buffer(NULL)
#endif
  {
  }
  bool is_null() const { return type == EMPTY_BUFFER; }
  GpuMemoryBufferType type;
  base::SharedMemoryHandle handle;
#if defined(OS_ANDROID)
  EGLClientBuffer native_buffer;
#endif
};

// Interface for creating and accessing a zero-copy GPU memory buffer.
// This design evolved from the generalization of GraphicBuffer API
// of Android framework.
//
// THREADING CONSIDERATIONS:
//
// This interface is thread-safe. However, multiple threads mapping
// a buffer for Write or ReadOrWrite simultaneously may result in undefined
// behavior and is not allowed.
class GFX_EXPORT GpuMemoryBuffer {
 public:
  enum AccessMode {
    READ_ONLY,
    WRITE_ONLY,
    READ_WRITE,
  };

  GpuMemoryBuffer();
  virtual ~GpuMemoryBuffer();

  // Maps the buffer so the client can write the bitmap data in |*vaddr|
  // subsequently. This call may block, for instance if the hardware needs
  // to finish rendering or if CPU caches need to be synchronized.
  virtual void Map(AccessMode mode, void** vaddr) = 0;

  // Unmaps the buffer. Called after all changes to the buffer are
  // completed.
  virtual void Unmap() = 0;

  // Returns true iff the buffer is mapped.
  virtual bool IsMapped() const = 0;

  // Returns the stride in bytes for the buffer.
  virtual uint32 GetStride() const = 0;

  // Returns a platform specific handle for this buffer.
  virtual GpuMemoryBufferHandle GetHandle() const = 0;
};

}  // namespace gfx

#endif  // UI_GFX_GPU_MEMORY_BUFFER_H_
