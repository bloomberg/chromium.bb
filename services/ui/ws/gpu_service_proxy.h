// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_GPU_SERVICE_PROXY_H_
#define SERVICES_UI_WS_GPU_SERVICE_PROXY_H_

#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/ui/gpu/gpu_main.h"
#include "services/ui/gpu/interfaces/gpu_service_internal.mojom.h"
#include "services/ui/public/interfaces/gpu_memory_buffer.mojom.h"
#include "services/ui/public/interfaces/gpu_service.mojom.h"

namespace ui {

class GpuServiceInternal;
class MusGpuMemoryBufferManager;

namespace ws {

class GpuServiceProxyDelegate;

// The proxy implementation that relays requests from clients to the real
// service implementation in the GPU process over mojom.GpuServiceInternal.
class GpuServiceProxy : public mojom::GpuService {
 public:
  explicit GpuServiceProxy(GpuServiceProxyDelegate* delegate);
  ~GpuServiceProxy() override;

  void Add(mojom::GpuServiceRequest request);

  // Requests a cc::mojom::DisplayCompositor interface from mus-gpu.
  void CreateDisplayCompositor(cc::mojom::DisplayCompositorRequest request,
                               cc::mojom::DisplayCompositorClientPtr client);

 private:
  void OnInitialized(const gpu::GPUInfo& gpu_info);
  void OnGpuChannelEstablished(const EstablishGpuChannelCallback& callback,
                               int32_t client_id,
                               mojo::ScopedMessagePipeHandle channel_handle);

  // mojom::GpuService overrides:
  void EstablishGpuChannel(
      const EstablishGpuChannelCallback& callback) override;
  void CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      const mojom::GpuService::CreateGpuMemoryBufferCallback& callback)
      override;
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              const gpu::SyncToken& sync_token) override;

  GpuServiceProxyDelegate* const delegate_;
  int32_t next_client_id_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  mojom::GpuServiceInternalPtr gpu_service_;
  mojo::BindingSet<mojom::GpuService> bindings_;
  gpu::GPUInfo gpu_info_;
  GpuMain gpu_main_;

  DISALLOW_COPY_AND_ASSIGN(GpuServiceProxy);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_GPU_SERVICE_PROXY_H_
