// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_PUBLIC_CPP_GPU_SHARED_WORKER_CONTEXT_PROVIDER_FACTORY_H_
#define SERVICES_WS_PUBLIC_CPP_GPU_SHARED_WORKER_CONTEXT_PROVIDER_FACTORY_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/ipc/common/surface_handle.h"
#include "url/gurl.h"

namespace gpu {
class GpuChannelHost;
class GpuMemoryBufferManager;

enum class ContextResult;
enum class SchedulingPriority;
}  // namespace gpu

namespace viz {
class RasterContextProvider;
}

namespace ws {

namespace command_buffer_metrics {
enum class ContextType;
}

// SharedWorkerContextProviderFactory is responsible for creation, and owning
// viz::RasterContextProvider.
class SharedWorkerContextProviderFactory {
 public:
  SharedWorkerContextProviderFactory(
      int32_t stream_id,
      gpu::SchedulingPriority priority,
      const GURL& identifying_url,
      ws::command_buffer_metrics::ContextType context_type);
  ~SharedWorkerContextProviderFactory();

  // Drops the reference to |provider_|. This ensures the next time Validate()
  // is called a new RasterContextProvider is created.
  void Reset();

  // Validates |provider_|, and if necessary attempts to recreate. Returns
  // creation status. Use provider() to access the created
  // RasterContextProvider.
  gpu::ContextResult Validate(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager);

  scoped_refptr<viz::RasterContextProvider> provider() { return provider_; }

 private:
  scoped_refptr<viz::RasterContextProvider> CreateContextProvider(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      gpu::SurfaceHandle surface_handle,
      bool need_alpha_channel,
      bool need_stencil_bits,
      bool support_locking,
      bool support_gles2_interface,
      bool support_raster_interface,
      bool support_grcontext,
      ws::command_buffer_metrics::ContextType type);

  const int32_t stream_id_;
  const gpu::SchedulingPriority priority_;
  const GURL identifying_url_;
  const ws::command_buffer_metrics::ContextType context_type_;
  scoped_refptr<viz::RasterContextProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerContextProviderFactory);
};

}  // namespace ws

#endif  // SERVICES_WS_PUBLIC_CPP_GPU_SHARED_WORKER_CONTEXT_PROVIDER_FACTORY_H_
