// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_GPU_HOST_H_
#define SERVICES_UI_WS_GPU_HOST_H_

#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/ui/gpu/gpu_main.h"
#include "services/ui/gpu/interfaces/gpu_host.mojom.h"
#include "services/ui/gpu/interfaces/gpu_service.mojom.h"
#include "services/ui/public/interfaces/gpu.mojom.h"

namespace ui {

class ServerGpuMemoryBufferManager;

namespace ws {

class GpuHostDelegate;

// Sets up connection from clients to the real service implementation in the GPU
// process.
class GpuHost : public mojom::GpuHost {
 public:
  explicit GpuHost(GpuHostDelegate* delegate);
  ~GpuHost() override;

  void Add(mojom::GpuRequest request);

  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget);
  void OnAcceleratedWidgetDestroyed(gfx::AcceleratedWidget widget);

  // Requests a cc::mojom::DisplayCompositor interface from mus-gpu.
  void CreateDisplayCompositor(cc::mojom::DisplayCompositorRequest request,
                               cc::mojom::DisplayCompositorClientPtr client);

 private:
  void OnBadMessageFromGpu();

  // mojom::GpuHost:
  void DidInitialize(const gpu::GPUInfo& gpu_info) override;
  void DidCreateOffscreenContext(const GURL& url) override;
  void DidDestroyOffscreenContext(const GURL& url) override;
  void DidDestroyChannel(int32_t client_id) override;
  void DidLoseContext(bool offscreen,
                      gpu::error::ContextLostReason reason,
                      const GURL& active_url) override;
  void SetChildSurface(gpu::SurfaceHandle parent,
                       gpu::SurfaceHandle child) override;
  void StoreShaderToDisk(int32_t client_id,
                         const std::string& key,
                         const std::string& shader) override;

  GpuHostDelegate* const delegate_;
  int32_t next_client_id_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  mojom::GpuServicePtr gpu_service_;
  mojo::Binding<mojom::GpuHost> gpu_host_binding_;
  gpu::GPUInfo gpu_info_;
  std::unique_ptr<ServerGpuMemoryBufferManager> gpu_memory_buffer_manager_;

  mojom::GpuMainPtr gpu_main_;

  // TODO(fsamuel): GpuHost should not be holding onto |gpu_main_impl|
  // because that will live in another process soon.
  std::unique_ptr<GpuMain> gpu_main_impl_;

  DISALLOW_COPY_AND_ASSIGN(GpuHost);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_GPU_HOST_H_
