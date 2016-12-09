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
#include "services/ui/gpu/interfaces/gpu_service_host.mojom.h"
#include "services/ui/gpu/interfaces/gpu_service_internal.mojom.h"
#include "services/ui/public/interfaces/gpu_service.mojom.h"

namespace ui {

class ServerGpuMemoryBufferManager;

namespace ws {

class GpuServiceProxyDelegate;

// Sets up connection from clients to the real service implementation in the GPU
// process.
class GpuServiceProxy : public mojom::GpuServiceHost {
 public:
  explicit GpuServiceProxy(GpuServiceProxyDelegate* delegate);
  ~GpuServiceProxy() override;

  void Add(mojom::GpuServiceRequest request);

  // Requests a cc::mojom::DisplayCompositor interface from mus-gpu.
  void CreateDisplayCompositor(cc::mojom::DisplayCompositorRequest request,
                               cc::mojom::DisplayCompositorClientPtr client);

 private:
  // mojom::GpuServiceHost:
  void DidInitialize(const gpu::GPUInfo& gpu_info) override;
  void DidCreateOffscreenContext(const GURL& url) override;
  void DidDestroyOffscreenContext(const GURL& url) override;
  void DidDestroyChannel(int32_t client_id) override;
  void DidLoseContext(bool offscreen,
                      gpu::error::ContextLostReason reason,
                      const GURL& active_url) override;
  void StoreShaderToDisk(int32_t client_id,
                         const std::string& key,
                         const std::string& shader) override;

  GpuServiceProxyDelegate* const delegate_;
  int32_t next_client_id_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  mojom::GpuServiceInternalPtr gpu_service_;
  mojo::Binding<mojom::GpuServiceHost> gpu_host_binding_;
  gpu::GPUInfo gpu_info_;
  std::unique_ptr<ServerGpuMemoryBufferManager> gpu_memory_buffer_manager_;

  mojom::GpuMainPtr gpu_main_;

  // TODO(fsamuel): GpuServiceProxy should not be holding onto |gpu_main_impl|
  // because that will live in another process soon.
  std::unique_ptr<GpuMain> gpu_main_impl_;

  DISALLOW_COPY_AND_ASSIGN(GpuServiceProxy);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_GPU_SERVICE_PROXY_H_
