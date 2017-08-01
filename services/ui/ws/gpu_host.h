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
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "services/ui/gpu/gpu_main.h"
#include "services/ui/gpu/interfaces/gpu_host.mojom.h"
#include "services/ui/public/interfaces/gpu.mojom.h"
#include "services/viz/gl/privileged/interfaces/gpu_service.mojom.h"

namespace viz {
class ServerGpuMemoryBufferManager;
}

namespace ui {

namespace ws {

class GpuClient;

namespace test {
class GpuHostTest;
}  // namespace test

class GpuHostDelegate;

// GpuHost sets up connection from clients to the real service implementation in
// the GPU process.
class GpuHost {
 public:
  GpuHost() = default;
  virtual ~GpuHost() = default;

  virtual void Add(mojom::GpuRequest request) = 0;
  virtual void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) = 0;
  virtual void OnAcceleratedWidgetDestroyed(gfx::AcceleratedWidget widget) = 0;

  // Requests a viz::mojom::FrameSinkManager interface from mus-gpu.
  virtual void CreateFrameSinkManager(
      viz::mojom::FrameSinkManagerRequest request,
      viz::mojom::FrameSinkManagerClientPtr client) = 0;
};

class DefaultGpuHost : public GpuHost, public mojom::GpuHost {
 public:
  explicit DefaultGpuHost(GpuHostDelegate* delegate);
  ~DefaultGpuHost() override;

 private:
  friend class test::GpuHostTest;

  GpuClient* AddInternal(mojom::GpuRequest request);
  void OnBadMessageFromGpu();

  // GpuHost:
  void Add(mojom::GpuRequest request) override;
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override;
  void OnAcceleratedWidgetDestroyed(gfx::AcceleratedWidget widget) override;
  void CreateFrameSinkManager(
      viz::mojom::FrameSinkManagerRequest request,
      viz::mojom::FrameSinkManagerClientPtr client) override;

  // mojom::GpuHost:
  void DidInitialize(const gpu::GPUInfo& gpu_info,
                     const gpu::GpuFeatureInfo& gpu_feature_info) override;
  void DidFailInitialize() override;
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
  void RecordLogMessage(int32_t severity,
                        const std::string& header,
                        const std::string& message) override;

  GpuHostDelegate* const delegate_;
  int32_t next_client_id_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  viz::mojom::GpuServicePtr gpu_service_;
  mojo::Binding<mojom::GpuHost> gpu_host_binding_;
  gpu::GPUInfo gpu_info_;
  std::unique_ptr<viz::ServerGpuMemoryBufferManager> gpu_memory_buffer_manager_;

  mojom::GpuMainPtr gpu_main_;

  // TODO(fsamuel): GpuHost should not be holding onto |gpu_main_impl|
  // because that will live in another process soon.
  std::unique_ptr<GpuMain> gpu_main_impl_;

  mojo::StrongBindingSet<mojom::Gpu> gpu_bindings_;

  DISALLOW_COPY_AND_ASSIGN(DefaultGpuHost);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_GPU_HOST_H_
