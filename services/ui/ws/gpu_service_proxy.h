// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_GPU_SERVICE_PROXY_H_
#define SERVICES_UI_WS_GPU_SERVICE_PROXY_H_

#include "gpu/config/gpu_info.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/ui/gpu/interfaces/gpu_service_internal.mojom.h"
#include "services/ui/public/interfaces/gpu_memory_buffer.mojom.h"
#include "services/ui/public/interfaces/gpu_service.mojom.h"

namespace ui {

class GpuServiceInternal;

// The proxy implementation that relays requests from clients to the real
// service implementation in the GPU process over mojom.GpuServiceInternal.
class GpuServiceProxy : public mojom::GpuService {
 public:
  GpuServiceProxy();
  ~GpuServiceProxy() override;

  void Add(mojom::GpuServiceRequest request);

 private:
  void OnInitialized(const gpu::GPUInfo& gpu_info);
  void OnGpuChannelEstablished(const EstablishGpuChannelCallback& callback,
                               int32_t client_id,
                               mojo::ScopedMessagePipeHandle channel_handle);

  // mojom::GpuService overrides:
  void EstablishGpuChannel(
      const EstablishGpuChannelCallback& callback) override;

  void CreateGpuMemoryBuffer(
      mojom::GpuMemoryBufferIdPtr id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      uint64_t surface_id,
      const mojom::GpuService::CreateGpuMemoryBufferCallback& callback)
      override;

  void DestroyGpuMemoryBuffer(mojom::GpuMemoryBufferIdPtr id,
                              const gpu::SyncToken& sync_token) override;

  int32_t next_client_id_;
  mojom::GpuServiceInternalPtr gpu_service_;
  mojo::BindingSet<GpuService> bindings_;
  gpu::GPUInfo gpu_info_;

  DISALLOW_COPY_AND_ASSIGN(GpuServiceProxy);
};

}  // namespace ui

#endif  // SERVICES_UI_WS_GPU_SERVICE_PROXY_H_
