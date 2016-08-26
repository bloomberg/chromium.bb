// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_GPU_GPU_SERVICE_INTERNAL_H_
#define SERVICES_UI_GPU_GPU_SERVICE_INTERNAL_H_

#include "base/callback.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_config.h"
#include "gpu/ipc/service/x_util.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ui/gpu/interfaces/gpu_service_internal.mojom.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace gpu {
class GpuChannelHost;
class GpuMemoryBufferFactory;
class SyncPointManager;
}

namespace media {
class MediaService;
}

namespace ui {

class MusGpuMemoryBufferManager;

// TODO(sad): GpuChannelManagerDelegate implementation should be in the gpu
// process, and the GpuChannelHostFactory should be in the host process (i.e.
// the window-server process). So the GpuChannelHostFactory parts of this should
// be split out.
class GpuServiceInternal : public gpu::GpuChannelManagerDelegate,
                           public gpu::GpuChannelHostFactory,
                           public mojom::GpuServiceInternal,
                           public base::NonThreadSafe {
 public:
  void Add(mojom::GpuServiceInternalRequest request);

  // TODO(sad): This should not be a singleton.
  static GpuServiceInternal* GetInstance();

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_local() const {
    return gpu_channel_local_;
  }

  gpu::GpuChannelManager* gpu_channel_manager() const {
    return gpu_channel_manager_.get();
  }

  gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory() const {
    return gpu_memory_buffer_factory_.get();
  }

 private:
  friend struct base::DefaultSingletonTraits<GpuServiceInternal>;

  GpuServiceInternal();
  ~GpuServiceInternal() override;

  void EstablishGpuChannelInternal(int32_t client_id,
                                   uint64_t client_tracing_id,
                                   bool preempts,
                                   bool allow_view_command_buffers,
                                   bool allow_real_time_streams,
                                   const EstablishGpuChannelCallback& callback);
  gfx::GpuMemoryBufferHandle CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      int client_id,
      gpu::SurfaceHandle surface_handle);
  gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferFromeHandle(
      gfx::GpuMemoryBufferHandle buffer_handle,
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      int client_id);
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              int client_id,
                              const gpu::SyncToken& sync_token);

  // gpu::GpuChannelManagerDelegate:
  void DidCreateOffscreenContext(const GURL& active_url) override;
  void DidDestroyChannel(int client_id) override;
  void DidDestroyOffscreenContext(const GURL& active_url) override;
  void DidLoseContext(bool offscreen,
                      gpu::error::ContextLostReason reason,
                      const GURL& active_url) override;
  void GpuMemoryUmaStats(const gpu::GPUMemoryUmaStats& params) override;
  void StoreShaderToDisk(int client_id,
                         const std::string& key,
                         const std::string& shader) override;
#if defined(OS_WIN)
  void SendAcceleratedSurfaceCreatedChildWindow(
      gpu::SurfaceHandle parent_window,
      gpu::SurfaceHandle child_window) override;
#endif
  void SetActiveURL(const GURL& url) override;

  // gpu::GpuChannelHostFactory:
  bool IsMainThread() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetIOThreadTaskRunner() override;
  std::unique_ptr<base::SharedMemory> AllocateSharedMemory(
      size_t size) override;

  // mojom::GpuServiceInternal:
  void Initialize(const InitializeCallback& callback) override;
  void EstablishGpuChannel(
      int32_t client_id,
      uint64_t client_tracing_id,
      const EstablishGpuChannelCallback& callback) override;

  void InitializeOnGpuThread(mojo::ScopedMessagePipeHandle* channel_handle,
                             base::WaitableEvent* event);
  void EstablishGpuChannelOnGpuThread(
      int client_id,
      uint64_t client_tracing_id,
      bool preempts,
      bool allow_view_command_buffers,
      bool allow_real_time_streams,
      mojo::ScopedMessagePipeHandle* channel_handle);

  // The main thread task runner.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // An event that will be signalled when we shutdown.
  base::WaitableEvent shutdown_event_;

  // The main thread for GpuService.
  base::Thread gpu_thread_;

  // The thread that handles IO events for GpuService.
  base::Thread io_thread_;

  std::unique_ptr<gpu::SyncPointManager> owned_sync_point_manager_;

  std::unique_ptr<gpu::GpuChannelManager> gpu_channel_manager_;

  std::unique_ptr<media::MediaService> media_service_;

  std::unique_ptr<gpu::GpuMemoryBufferFactory> gpu_memory_buffer_factory_;

  // A GPU memory buffer manager used locally.
  std::unique_ptr<MusGpuMemoryBufferManager> gpu_memory_buffer_manager_local_;

  // A GPU channel used locally.
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_local_;

  gpu::GpuPreferences gpu_preferences_;

  // Information about the GPU, such as device and vendor ID.
  gpu::GPUInfo gpu_info_;

  mojo::StrongBinding<mojom::GpuServiceInternal> binding_;

  DISALLOW_COPY_AND_ASSIGN(GpuServiceInternal);
};

}  // namespace ui

#endif  // SERVICES_UI_GPU_GPU_SERVICE_INTERNAL_H_
