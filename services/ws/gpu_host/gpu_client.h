// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_GPU_HOST_GPU_CLIENT_H_
#define SERVICES_WS_GPU_HOST_GPU_CLIENT_H_

#include "base/memory/weak_ptr.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/ws/public/mojom/gpu.mojom.h"

namespace viz {
namespace mojom {
class GpuService;
}  // namespace mojom

class HostGpuMemoryBufferManager;
}  // namespace viz

namespace ws {
namespace gpu_host {

namespace test {
class GpuHostTest;
}  // namespace test

// The implementation that relays requests from clients to the real
// service implementation in the GPU process over mojom.GpuService.
class GpuClient : public mojom::GpuMemoryBufferFactory, public mojom::Gpu {
 public:
  GpuClient(int client_id,
            gpu::GPUInfo* gpu_info,
            gpu::GpuFeatureInfo* gpu_feature_info,
            viz::HostGpuMemoryBufferManager* gpu_memory_buffer_manager,
            viz::mojom::GpuService* gpu_service);
  ~GpuClient() override;

 private:
  friend class test::GpuHostTest;

  // EstablishGpuChannelCallback:
  void OnGpuChannelEstablished(mojo::ScopedMessagePipeHandle channel_handle);

  // mojom::GpuMemoryBufferFactory overrides:
  void CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      mojom::GpuMemoryBufferFactory::CreateGpuMemoryBufferCallback callback)
      override;
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              const gpu::SyncToken& sync_token) override;

  // mojom::Gpu overrides:
  void CreateGpuMemoryBufferFactory(
      mojom::GpuMemoryBufferFactoryRequest request) override;
  void EstablishGpuChannel(EstablishGpuChannelCallback callback) override;
  void CreateJpegDecodeAccelerator(
      media::mojom::JpegDecodeAcceleratorRequest jda_request) override;
  void CreateVideoEncodeAcceleratorProvider(
      media::mojom::VideoEncodeAcceleratorProviderRequest vea_provider_request)
      override;

  const int client_id_;
  mojo::BindingSet<mojom::GpuMemoryBufferFactory>
      gpu_memory_buffer_factory_bindings_;

  // The objects these pointers refer to are owned by the GpuHost object.
  const gpu::GPUInfo* gpu_info_;
  const gpu::GpuFeatureInfo* gpu_feature_info_;
  viz::HostGpuMemoryBufferManager* gpu_memory_buffer_manager_;
  viz::mojom::GpuService* gpu_service_;
  EstablishGpuChannelCallback establish_callback_;

  base::WeakPtrFactory<GpuClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuClient);
};

}  // namespace gpu_host
}  // namespace ws

#endif  // SERVICES_WS_GPU_HOST_GPU_CLIENT_H_
