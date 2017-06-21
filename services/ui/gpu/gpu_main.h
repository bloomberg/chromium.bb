// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_GPU_GPU_MAIN_H_
#define SERVICES_UI_GPU_GPU_MAIN_H_

#include "base/power_monitor/power_monitor.h"
#include "base/threading/thread.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "gpu/ipc/service/gpu_init.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/gpu/interfaces/gpu_main.mojom.h"
#include "services/ui/gpu/interfaces/gpu_service.mojom.h"

namespace gpu {
class GpuMemoryBufferFactory;
}

namespace viz {
class DisplayProvider;
class MojoFrameSinkManager;
}

namespace ui {

class GpuService;

class GpuMain : public gpu::GpuSandboxHelper, public mojom::GpuMain {
 public:
  explicit GpuMain(mojom::GpuMainRequest request);
  ~GpuMain() override;

  // mojom::GpuMain implementation:
  void CreateGpuService(mojom::GpuServiceRequest request,
                        mojom::GpuHostPtr gpu_host,
                        const gpu::GpuPreferences& preferences,
                        mojo::ScopedSharedBufferHandle activity_flags) override;
  void CreateFrameSinkManager(
      cc::mojom::FrameSinkManagerRequest request,
      cc::mojom::FrameSinkManagerClientPtr client) override;

  void OnStart();

  GpuService* gpu_service() { return gpu_service_.get(); }

 private:
  void BindOnGpu(mojom::GpuMainRequest request);
  void InitOnGpuThread(
      scoped_refptr<base::SingleThreadTaskRunner> io_runner,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_runner);

  void CreateFrameSinkManagerInternal(
      cc::mojom::FrameSinkManagerRequest request,
      cc::mojom::FrameSinkManagerClientPtrInfo client_info);
  void CreateFrameSinkManagerOnCompositorThread(
      cc::mojom::FrameSinkManagerRequest request,
      cc::mojom::FrameSinkManagerClientPtrInfo client_info);
  void CreateGpuServiceOnGpuThread(mojom::GpuServiceRequest request,
                                   mojom::GpuHostPtr gpu_host,
                                   const gpu::GpuPreferences& preferences,
                                   gpu::GpuProcessActivityFlags activity_flags);

  void TearDownOnCompositorThread();
  void TearDownOnGpuThread();

  // gpu::GpuSandboxHelper:
  void PreSandboxStartup() override;
  bool EnsureSandboxInitialized(
      gpu::GpuWatchdogThread* watchdog_thread) override;

  std::unique_ptr<gpu::GpuInit> gpu_init_;
  std::unique_ptr<GpuService> gpu_service_;

  // The InCommandCommandBuffer::Service used by the frame sink manager.
  scoped_refptr<gpu::InProcessCommandBuffer::Service> gpu_command_service_;

  // If the gpu service is not yet ready then we stash pending MessagePipes in
  // these member variables.
  cc::mojom::FrameSinkManagerRequest pending_frame_sink_manager_request_;
  cc::mojom::FrameSinkManagerClientPtrInfo
      pending_frame_sink_manager_client_info_;

  // Provides mojo interfaces for creating and managing FrameSinks.
  std::unique_ptr<viz::MojoFrameSinkManager> frame_sink_manager_;
  std::unique_ptr<viz::DisplayProvider> display_provider_;

  std::unique_ptr<gpu::GpuMemoryBufferFactory> gpu_memory_buffer_factory_;

  // The main thread for Gpu.
  base::Thread gpu_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_thread_task_runner_;

  // The thread that handles IO events for Gpu.
  base::Thread io_thread_;

  // The frame sink manager gets its own thread in mus-gpu. The gpu service,
  // where GL commands are processed resides on its own thread. Various
  // components of the frame sink manager such as Display, ResourceProvider,
  // and GLRenderer block on sync tokens from other command buffers. Thus,
  // the gpu service must live on a separate thread.
  base::Thread compositor_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_thread_task_runner_;

  base::PowerMonitor power_monitor_;
  mojo::Binding<mojom::GpuMain> binding_;

  DISALLOW_COPY_AND_ASSIGN(GpuMain);
};

}  // namespace ui

#endif  // SERVICES_UI_GPU_GPU_MAIN_H_
