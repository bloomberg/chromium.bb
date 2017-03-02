// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/gpu/gpu_service.h"

#include "base/bind.h"
#include "base/debug/crash_logging.h"
#include "base/memory/shared_memory.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/output/in_process_context_provider.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/common/memory_stats.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_message_filter.h"
#include "media/gpu/ipc/service/gpu_jpeg_decode_accelerator.h"
#include "media/gpu/ipc/service/gpu_video_decode_accelerator.h"
#include "media/gpu/ipc/service/gpu_video_encode_accelerator.h"
#include "media/gpu/ipc/service/media_gpu_channel_manager.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/init/gl_factory.h"
#include "url/gurl.h"

namespace ui {

GpuService::GpuService(const gpu::GPUInfo& gpu_info,
                       std::unique_ptr<gpu::GpuWatchdogThread> watchdog_thread,
                       gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
                       scoped_refptr<base::SingleThreadTaskRunner> io_runner,
                       const gpu::GpuFeatureInfo& gpu_feature_info)
    : io_runner_(std::move(io_runner)),
      shutdown_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
      watchdog_thread_(std::move(watchdog_thread)),
      gpu_memory_buffer_factory_(gpu_memory_buffer_factory),
      gpu_info_(gpu_info),
      gpu_feature_info_(gpu_feature_info),
      sync_point_manager_(nullptr) {}

GpuService::~GpuService() {
  bindings_.CloseAllBindings();
  media_gpu_channel_manager_.reset();
  gpu_channel_manager_.reset();
  owned_sync_point_manager_.reset();

  // Signal this event before destroying the child process.  That way all
  // background threads can cleanup.
  // For example, in the renderer the RenderThread instances will be able to
  // notice shutdown before the render process begins waiting for them to exit.
  shutdown_event_.Signal();
}

void GpuService::InitializeWithHost(mojom::GpuHostPtr gpu_host,
                                    const gpu::GpuPreferences& preferences,
                                    gpu::SyncPointManager* sync_point_manager,
                                    base::WaitableEvent* shutdown_event) {
  DCHECK(CalledOnValidThread());
  DCHECK(!gpu_host_);
  gpu_host_ = std::move(gpu_host);
  gpu_preferences_ = preferences;
  gpu_info_.video_decode_accelerator_capabilities =
      media::GpuVideoDecodeAccelerator::GetCapabilities(gpu_preferences_);
  gpu_info_.video_encode_accelerator_supported_profiles =
      media::GpuVideoEncodeAccelerator::GetSupportedProfiles(gpu_preferences_);
  gpu_info_.jpeg_decode_accelerator_supported =
      media::GpuJpegDecodeAcceleratorFactoryProvider::
          IsAcceleratedJpegDecodeSupported();
  gpu_host_->DidInitialize(gpu_info_);

  sync_point_manager_ = sync_point_manager;
  if (!sync_point_manager_) {
    owned_sync_point_manager_ = base::MakeUnique<gpu::SyncPointManager>();
    sync_point_manager_ = owned_sync_point_manager_.get();
  }

  // Defer creation of the render thread. This is to prevent it from handling
  // IPC messages before the sandbox has been enabled and all other necessary
  // initialization has succeeded.
  gpu_channel_manager_.reset(new gpu::GpuChannelManager(
      gpu_preferences_, this, watchdog_thread_.get(),
      base::ThreadTaskRunnerHandle::Get().get(), io_runner_.get(),
      shutdown_event ? shutdown_event : &shutdown_event_, sync_point_manager_,
      gpu_memory_buffer_factory_, gpu_feature_info_));

  media_gpu_channel_manager_.reset(
      new media::MediaGpuChannelManager(gpu_channel_manager_.get()));
}

void GpuService::Bind(mojom::GpuServiceRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void GpuService::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    gpu::SurfaceHandle surface_handle,
    const CreateGpuMemoryBufferCallback& callback) {
  DCHECK(CalledOnValidThread());
  callback.Run(gpu_memory_buffer_factory_->CreateGpuMemoryBuffer(
      id, size, format, usage, client_id, surface_handle));
}

void GpuService::DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                        int client_id,
                                        const gpu::SyncToken& sync_token) {
  DCHECK(CalledOnValidThread());
  if (gpu_channel_manager_)
    gpu_channel_manager_->DestroyGpuMemoryBuffer(id, client_id, sync_token);
}

void GpuService::GetVideoMemoryUsageStats(
    const GetVideoMemoryUsageStatsCallback& callback) {
  gpu::VideoMemoryUsageStats video_memory_usage_stats;
  if (gpu_channel_manager_) {
    gpu_channel_manager_->gpu_memory_manager()->GetVideoMemoryUsageStats(
        &video_memory_usage_stats);
  }
  callback.Run(video_memory_usage_stats);
}

void GpuService::DidCreateOffscreenContext(const GURL& active_url) {
  gpu_host_->DidCreateOffscreenContext(active_url);
}

void GpuService::DidDestroyChannel(int client_id) {
  media_gpu_channel_manager_->RemoveChannel(client_id);
  gpu_host_->DidDestroyChannel(client_id);
}

void GpuService::DidDestroyOffscreenContext(const GURL& active_url) {
  gpu_host_->DidDestroyOffscreenContext(active_url);
}

void GpuService::DidLoseContext(bool offscreen,
                                gpu::error::ContextLostReason reason,
                                const GURL& active_url) {
  gpu_host_->DidLoseContext(offscreen, reason, active_url);
}

void GpuService::StoreShaderToDisk(int client_id,
                                   const std::string& key,
                                   const std::string& shader) {
  gpu_host_->StoreShaderToDisk(client_id, key, shader);
}

#if defined(OS_WIN)
void GpuService::SendAcceleratedSurfaceCreatedChildWindow(
    gpu::SurfaceHandle parent_window,
    gpu::SurfaceHandle child_window) {
  gpu_host_->SetChildSurface(parent_window, child_window);
}
#endif

void GpuService::SetActiveURL(const GURL& url) {
  constexpr char kActiveURL[] = "url-chunk";
  base::debug::SetCrashKeyValue(kActiveURL, url.possibly_invalid_spec());
}

void GpuService::EstablishGpuChannel(
    int32_t client_id,
    uint64_t client_tracing_id,
    bool is_gpu_host,
    const EstablishGpuChannelCallback& callback) {
  DCHECK(CalledOnValidThread());

  if (!gpu_channel_manager_) {
    callback.Run(mojo::ScopedMessagePipeHandle());
    return;
  }

  const bool preempts = is_gpu_host;
  const bool allow_view_command_buffers = is_gpu_host;
  const bool allow_real_time_streams = is_gpu_host;
  mojo::ScopedMessagePipeHandle channel_handle;
  IPC::ChannelHandle handle = gpu_channel_manager_->EstablishChannel(
      client_id, client_tracing_id, preempts, allow_view_command_buffers,
      allow_real_time_streams);
  channel_handle.reset(handle.mojo_handle);
  media_gpu_channel_manager_->AddChannel(client_id);
  callback.Run(std::move(channel_handle));
}

}  // namespace ui
