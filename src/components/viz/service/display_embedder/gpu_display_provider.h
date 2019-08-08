// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GPU_DISPLAY_PROVIDER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GPU_DISPLAY_PROVIDER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/display_embedder/display_provider.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/in_process_command_buffer.h"

#if defined(OS_WIN)
#include "components/viz/service/display_embedder/output_device_backing.h"
#endif

namespace gpu {
class CommandBufferTaskExecutor;
class GpuChannelManagerDelegate;
class GpuMemoryBufferManager;
class ImageFactory;
class SharedContextState;
}  // namespace gpu

namespace viz {
class Display;
class GpuServiceImpl;
class ServerSharedBitmapManager;
class SoftwareOutputDevice;

// In-process implementation of DisplayProvider.
class VIZ_SERVICE_EXPORT GpuDisplayProvider : public DisplayProvider {
 public:
  GpuDisplayProvider(
      uint32_t restart_id,
      GpuServiceImpl* gpu_service_impl,
      gpu::CommandBufferTaskExecutor* task_executor,
      gpu::GpuChannelManagerDelegate* gpu_channel_manager_delegate,
      std::unique_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager,
      gpu::ImageFactory* image_factory,
      ServerSharedBitmapManager* server_shared_bitmap_manager,
      bool headless,
      bool wait_for_all_pipeline_stages_before_draw);
  // Software compositing only.
  GpuDisplayProvider(uint32_t restart_id,
                     ServerSharedBitmapManager* server_shared_bitmap_manager,
                     bool headless,
                     bool wait_for_all_pipeline_stages_before_draw);
  ~GpuDisplayProvider() override;

  // DisplayProvider implementation.
  std::unique_ptr<Display> CreateDisplay(
      const FrameSinkId& frame_sink_id,
      gpu::SurfaceHandle surface_handle,
      bool gpu_compositing,
      mojom::DisplayClient* display_client,
      BeginFrameSource* begin_frame_source,
      UpdateVSyncParametersCallback update_vsync_callback,
      const RendererSettings& renderer_settings,
      bool send_swap_size_notifications) override;
  uint32_t GetRestartId() const override;

 private:
  std::unique_ptr<SoftwareOutputDevice> CreateSoftwareOutputDeviceForPlatform(
      gpu::SurfaceHandle surface_handle,
      mojom::DisplayClient* display_client);

  const uint32_t restart_id_;
  GpuServiceImpl* const gpu_service_impl_;
  gpu::CommandBufferTaskExecutor* const task_executor_;
  gpu::GpuChannelManagerDelegate* const gpu_channel_manager_delegate_;
  std::unique_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager_;
  gpu::ImageFactory* const image_factory_;
  ServerSharedBitmapManager* const server_shared_bitmap_manager_;

#if defined(OS_WIN)
  // Used for software compositing output on Windows.
  OutputDeviceBacking output_device_backing_;
#endif

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // A shared context which will be used on display compositor thread.
  scoped_refptr<gpu::SharedContextState> shared_context_state_;
  std::unique_ptr<gpu::MailboxManager> mailbox_manager_;
  std::unique_ptr<gpu::SyncPointManager> sync_point_manager_;

  const bool headless_;
  const bool wait_for_all_pipeline_stages_before_draw_;

  DISALLOW_COPY_AND_ASSIGN(GpuDisplayProvider);
};

}  // namespace viz

#endif  //  COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GPU_DISPLAY_PROVIDER_H_
