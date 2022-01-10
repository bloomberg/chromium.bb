// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_
#define GPU_IPC_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_pool.h"
#include "build/build_config.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(USE_OZONE)
namespace gfx {
class ClientNativePixmapFactory;
}
#endif

namespace gpu {

class GpuMemoryBufferManager;

// Provides a common factory for GPU memory buffer implementations.
class GPU_EXPORT GpuMemoryBufferSupport {
 public:
  GpuMemoryBufferSupport();

  GpuMemoryBufferSupport(const GpuMemoryBufferSupport&) = delete;
  GpuMemoryBufferSupport& operator=(const GpuMemoryBufferSupport&) = delete;

  virtual ~GpuMemoryBufferSupport();

  // Returns the native GPU memory buffer factory type. Returns EMPTY_BUFFER
  // type if native buffers are not supported.
  gfx::GpuMemoryBufferType GetNativeGpuMemoryBufferType();

  // Returns whether the provided buffer format is supported.
  bool IsNativeGpuMemoryBufferConfigurationSupported(gfx::BufferFormat format,
                                                     gfx::BufferUsage usage);

#if defined(USE_OZONE)
  gfx::ClientNativePixmapFactory* client_native_pixmap_factory() {
    return client_native_pixmap_factory_.get();
  }
#endif

  // Returns whether the provided buffer format is supported.
  bool IsConfigurationSupportedForTest(gfx::GpuMemoryBufferType type,
                                       gfx::BufferFormat format,
                                       gfx::BufferUsage usage);

  // Creates a GpuMemoryBufferImpl from the given |handle|. |size| and |format|
  // should match what was used to allocate the |handle|. |callback|, if
  // non-null, is called when instance is deleted, which is not necessarily on
  // the same thread as this function was called on and instance was created on.
  // |gpu_memory_buffer_manager| and |pool| are only needed if the created
  // buffer is a windows DXGI buffer and it needs to be mapped at the consumer.
  virtual std::unique_ptr<GpuMemoryBufferImpl>
  CreateGpuMemoryBufferImplFromHandle(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      GpuMemoryBufferImpl::DestructionCallback callback,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager = nullptr,
      scoped_refptr<base::UnsafeSharedMemoryPool> pool = nullptr);

 private:
#if defined(USE_OZONE)
  std::unique_ptr<gfx::ClientNativePixmapFactory> client_native_pixmap_factory_;
#endif
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_MEMORY_BUFFER_SUPPORT_H_
