// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GPU_MEMORY_BUFFER_H_
#define UI_GFX_GPU_MEMORY_BUFFER_H_

#include "base/memory/shared_memory.h"
#include "build/build_config.h"
#include "ui/gfx/gfx_export.h"

extern "C" typedef struct _ClientBuffer* ClientBuffer;

namespace gfx {

enum GpuMemoryBufferType {
  EMPTY_BUFFER,
  SHARED_MEMORY_BUFFER,
  IO_SURFACE_BUFFER,
  SURFACE_TEXTURE_BUFFER,
  OZONE_NATIVE_BUFFER,
  GPU_MEMORY_BUFFER_TYPE_LAST = OZONE_NATIVE_BUFFER
};

using GpuMemoryBufferId = int32;

struct GFX_EXPORT GpuMemoryBufferHandle {
  GpuMemoryBufferHandle();
  bool is_null() const { return type == EMPTY_BUFFER; }
  GpuMemoryBufferType type;
  GpuMemoryBufferId id;
  base::SharedMemoryHandle handle;
#if defined(OS_MACOSX)
  uint32 io_surface_id;
#endif
};

// This interface typically correspond to a type of shared memory that is also
// shared with the GPU. A GPU memory buffer can be written to directly by
// regular CPU code, but can also be read by the GPU.
class GFX_EXPORT GpuMemoryBuffer {
 public:
  // The format needs to be taken into account when mapping a buffer into the
  // client's address space.
  enum Format {
    ATC,
    ATCIA,
    DXT1,
    DXT5,
    ETC1,
    RGBA_8888,
    RGBX_8888,
    BGRA_8888,

    FORMAT_LAST = BGRA_8888
  };

  // The usage mode affects how a buffer can be used. Only buffers created with
  // MAP can be mapped into the client's address space and accessed by the CPU.
  enum Usage { MAP, SCANOUT, USAGE_LAST = SCANOUT };

  virtual ~GpuMemoryBuffer() {}

  // Maps the buffer into the client's address space so it can be written to by
  // the CPU. This call may block, for instance if the GPU needs to finish
  // accessing the buffer or if CPU caches need to be synchronized. Returns NULL
  // on failure.
  virtual void* Map() = 0;

  // Unmaps the buffer. It's illegal to use the pointer returned by Map() after
  // this has been called.
  virtual void Unmap() = 0;

  // Returns true iff the buffer is mapped.
  virtual bool IsMapped() const = 0;

  // Returns the format for the buffer.
  virtual Format GetFormat() const = 0;

  // Returns the stride in bytes for the buffer.
  virtual uint32 GetStride() const = 0;

  // Returns a platform specific handle for this buffer.
  virtual GpuMemoryBufferHandle GetHandle() const = 0;

  // Type-checking downcast routine.
  virtual ClientBuffer AsClientBuffer() = 0;
};

}  // namespace gfx

#endif  // UI_GFX_GPU_MEMORY_BUFFER_H_
