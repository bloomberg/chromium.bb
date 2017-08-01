// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_GPU_CLIENT_H_
#define SERVICES_UI_WS_GPU_CLIENT_H_

#include "base/memory/weak_ptr.h"
#include "gpu/config/gpu_info.h"
#include "services/ui/public/interfaces/gpu.mojom.h"

namespace viz {
namespace mojom {
class GpuService;
}  // namespace mojom

class ServerGpuMemoryBufferManager;
}

namespace ui {

namespace ws {

namespace test {
class GpuHostTest;
}  // namespace test

// The implementation that relays requests from clients to the real
// service implementation in the GPU process over mojom.GpuService.
class GpuClient : public mojom::Gpu {
 public:
  GpuClient(int client_id,
            gpu::GPUInfo* gpu_info,
            viz::ServerGpuMemoryBufferManager* gpu_memory_buffer_manager,
            viz::mojom::GpuService* gpu_service);
  ~GpuClient() override;

 private:
  friend class test::GpuHostTest;

  // EstablishGpuChannelCallback:
  void OnGpuChannelEstablished(const EstablishGpuChannelCallback& callback,
                               mojo::ScopedMessagePipeHandle channel_handle);
  // mojom::Gpu overrides:
  void EstablishGpuChannel(
      const EstablishGpuChannelCallback& callback) override;
  void CreateJpegDecodeAccelerator(
      media::mojom::GpuJpegDecodeAcceleratorRequest jda_request) override;
  void CreateVideoEncodeAcceleratorProvider(
      media::mojom::VideoEncodeAcceleratorProviderRequest request) override;
  void CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      const mojom::Gpu::CreateGpuMemoryBufferCallback& callback) override;
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              const gpu::SyncToken& sync_token) override;

  const int client_id_;

  // The objects these pointers refer to are owned by the GpuHost object.
  const gpu::GPUInfo* gpu_info_;
  viz::ServerGpuMemoryBufferManager* gpu_memory_buffer_manager_;
  viz::mojom::GpuService* gpu_service_;

  base::WeakPtrFactory<GpuClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuClient);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_GPU_CLIENT_H_
