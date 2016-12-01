// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_GPU_GPU_MAIN_H_
#define SERVICES_UI_GPU_GPU_MAIN_H_

#include "base/threading/thread.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "gpu/ipc/service/gpu_init.h"
#include "services/ui/gpu/interfaces/gpu_main.mojom.h"
#include "services/ui/gpu/interfaces/gpu_service_internal.mojom.h"
#include "services/ui/surfaces/display_compositor.h"

namespace gpu {
class GpuMemoryBufferFactory;
class ImageFactory;
}

namespace ui {

class GpuServiceInternal;

class GpuMain : public gpu::GpuSandboxHelper, public mojom::GpuMain {
 public:
  explicit GpuMain(mojom::GpuMainRequest request);
  ~GpuMain() override;

  // mojom::GpuMain implementation:
  void CreateGpuService(mojom::GpuServiceInternalRequest request,
                        const CreateGpuServiceCallback& callback) override;
  void CreateDisplayCompositor(
      cc::mojom::DisplayCompositorRequest request,
      cc::mojom::DisplayCompositorClientPtr client) override;

  void OnStart();

  GpuServiceInternal* gpu_service() { return gpu_service_internal_.get(); }

 private:
  void InitOnGpuThread(
      scoped_refptr<base::SingleThreadTaskRunner> io_runner,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_runner);

  void CreateDisplayCompositorInternal(
      cc::mojom::DisplayCompositorRequest request,
      cc::mojom::DisplayCompositorClientPtrInfo client_info);
  void CreateDisplayCompositorOnCompositorThread(
      gpu::ImageFactory* image_factory,
      mojom::GpuServiceInternalPtrInfo gpu_service_info,
      cc::mojom::DisplayCompositorRequest request,
      cc::mojom::DisplayCompositorClientPtrInfo client_info);
  void CreateGpuServiceOnGpuThread(
      mojom::GpuServiceInternalRequest request,
      scoped_refptr<base::SingleThreadTaskRunner> origin_runner,
      const CreateGpuServiceCallback& callback);

  void TearDownOnCompositorThread();
  void TearDownOnGpuThread();

  // gpu::GpuSandboxHelper:
  void PreSandboxStartup() override;
  bool EnsureSandboxInitialized(
      gpu::GpuWatchdogThread* watchdog_thread) override;

  std::unique_ptr<gpu::GpuInit> gpu_init_;
  std::unique_ptr<GpuServiceInternal> gpu_service_internal_;

  // The message-pipe used by the DisplayCompositor to request gpu memory
  // buffers.
  mojom::GpuServiceInternalPtr gpu_internal_;

  // The InCommandCommandBuffer::Service used by the display compositor.
  scoped_refptr<gpu::InProcessCommandBuffer::Service> gpu_command_service_;

  // If the gpu service is not yet ready then we stash pending MessagePipes in
  // these member variables.
  cc::mojom::DisplayCompositorRequest pending_display_compositor_request_;
  cc::mojom::DisplayCompositorClientPtrInfo
      pending_display_compositor_client_info_;

  std::unique_ptr<DisplayCompositor> display_compositor_;

  std::unique_ptr<gpu::GpuMemoryBufferFactory> gpu_memory_buffer_factory_;

  // The main thread for GpuService.
  base::Thread gpu_thread_;

  // The thread that handles IO events for GpuService.
  base::Thread io_thread_;

  // The display compositor gets its own thread in mus-gpu. The gpu service,
  // where GL commands are processed resides on its own thread. Various
  // components of the display compositor such as Display, ResourceProvider,
  // and GLRenderer block on sync tokens from other command buffers. Thus,
  // the gpu service must live on a separate thread.
  base::Thread compositor_thread_;

  mojo::Binding<mojom::GpuMain> binding_;

  DISALLOW_COPY_AND_ASSIGN(GpuMain);
};

}  // namespace ui

#endif  // SERVICES_UI_GPU_GPU_MAIN_H_
