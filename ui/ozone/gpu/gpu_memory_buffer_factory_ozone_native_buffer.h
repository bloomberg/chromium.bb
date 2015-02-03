// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_GPU_GPU_MEMORY_BUFFER_FACTORY_OZONE_NATIVE_BUFFER_H_
#define UI_OZONE_GPU_GPU_MEMORY_BUFFER_FACTORY_OZONE_NATIVE_BUFFER_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/gpu/ozone_gpu_export.h"
#include "ui/ozone/public/native_pixmap.h"

namespace gfx {
class GLImage;
}

namespace ui {
class NativePixmap;

class OZONE_GPU_EXPORT GpuMemoryBufferFactoryOzoneNativeBuffer {
  typedef std::map<std::pair<uint32_t, uint32_t>, scoped_refptr<NativePixmap> >
      BufferToPixmapMap;

 public:
  GpuMemoryBufferFactoryOzoneNativeBuffer();
  virtual ~GpuMemoryBufferFactoryOzoneNativeBuffer();

  // Creates a GPU memory buffer identified by |id|.
  bool CreateGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                             const gfx::Size& size,
                             gfx::GpuMemoryBuffer::Format format,
                             gfx::GpuMemoryBuffer::Usage usage,
                             int client_id,
                             gfx::PluginWindowHandle surface_handle);

  // Destroys GPU memory buffer identified by |id|.
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id, int client_id);

  // Creates a GLImage instance for GPU memory buffer identified by |id|.
  scoped_refptr<gfx::GLImage> CreateImageForGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      unsigned internalformat,
      int client_id);

  static scoped_refptr<gfx::GLImage> CreateImageForPixmap(
      scoped_refptr<NativePixmap> pixmap,
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      unsigned internalformat);

 private:
  BufferToPixmapMap native_pixmap_map_;
  base::Lock native_pixmap_map_lock_;
};

}  // namespace ui

#endif  // UI_OZONE_GPU_GPU_MEMORY_BUFFER_FACTORY_OZONE_NATIVE_BUFFER_H_
