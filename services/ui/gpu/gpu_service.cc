// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/gpu/gpu_service.h"

#include "base/bind.h"
#include "base/debug/crash_logging.h"
#include "base/lazy_instance.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
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

#if defined(OS_ANDROID)
#include "base/android/throw_uncaught_exception.h"
#include "media/gpu/avda_codec_allocator.h"
#endif

namespace ui {

namespace {

static base::LazyInstance<base::Callback<
    void(int severity, size_t message_start, const std::string& message)>>::
    Leaky g_log_callback = LAZY_INSTANCE_INITIALIZER;

bool GpuLogMessageHandler(int severity,
                          const char* file,
                          int line,
                          size_t message_start,
                          const std::string& message) {
  g_log_callback.Get().Run(severity, message_start, message);
  return false;
}

}  // namespace

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
  logging::SetLogMessageHandler(nullptr);
  g_log_callback.Get() =
      base::Callback<void(int, size_t, const std::string&)>();
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

void GpuService::UpdateGPUInfoFromPreferences(
    const gpu::GpuPreferences& preferences) {
  DCHECK(CalledOnValidThread());
  DCHECK(!gpu_host_);
  gpu_preferences_ = preferences;
  gpu_info_.video_decode_accelerator_capabilities =
      media::GpuVideoDecodeAccelerator::GetCapabilities(gpu_preferences_);
  gpu_info_.video_encode_accelerator_supported_profiles =
      media::GpuVideoEncodeAccelerator::GetSupportedProfiles(gpu_preferences_);
  gpu_info_.jpeg_decode_accelerator_supported =
      media::GpuJpegDecodeAcceleratorFactoryProvider::
          IsAcceleratedJpegDecodeSupported();
  // Record initialization only after collecting the GPU info because that can
  // take a significant amount of time.
  gpu_info_.initialization_time = base::Time::Now() - start_time_;
}

void GpuService::InitializeWithHost(mojom::GpuHostPtr gpu_host,
                                    const gpu::GpuPreferences& preferences,
                                    gpu::GpuProcessActivityFlags activity_flags,
                                    gpu::SyncPointManager* sync_point_manager,
                                    base::WaitableEvent* shutdown_event) {
  gpu_host->DidInitialize(gpu_info_);
  gpu_host_ =
      mojom::ThreadSafeGpuHostPtr::Create(gpu_host.PassInterface(), io_runner_);
  if (!in_host_process_) {
    // The global callback is reset from the dtor. So Unretained() here is safe.
    // Note that the callback can be called from any thread. Consequently, the
    // callback cannot use a WeakPtr.
    g_log_callback.Get() =
        base::Bind(&GpuService::RecordLogMessage, base::Unretained(this));
    logging::SetLogMessageHandler(GpuLogMessageHandler);
  }

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
      gpu_memory_buffer_factory_, gpu_feature_info_,
      std::move(activity_flags)));

  media_gpu_channel_manager_.reset(
      new media::MediaGpuChannelManager(gpu_channel_manager_.get()));
  if (watchdog_thread())
    watchdog_thread()->AddPowerObserver();
}

void GpuService::Bind(mojom::GpuServiceRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void GpuService::RecordLogMessage(int severity,
                                  size_t message_start,
                                  const std::string& str) {
  std::string header = str.substr(0, message_start);
  std::string message = str.substr(message_start);
  (*gpu_host_)->RecordLogMessage(severity, header, message);
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

void GpuService::RequestCompleteGpuInfo(
    const RequestCompleteGpuInfoCallback& callback) {
  UpdateGpuInfoPlatform();
  callback.Run(gpu_info_);
#if defined(OS_WIN)
  if (!in_host_process_) {
    // The unsandboxed GPU process fulfilled its duty. Rest in peace.
    base::MessageLoop::current()->QuitWhenIdle();
  }
#endif
}

#if defined(OS_MACOSX)
void GpuService::UpdateGpuInfoPlatform() {
  // gpu::CollectContextGraphicsInfo() is already called during gpu process
  // initialization (see GpuInit::InitializeAndStartSandbox()) on non-mac
  // platforms, and during in-browser gpu thread initialization on all platforms
  // (See InProcessGpuThread::Init()).
  if (in_host_process_)
    return;

  DCHECK_EQ(gpu::kCollectInfoNone, gpu_info_.context_info_state);
  gpu::CollectInfoResult result = gpu::CollectContextGraphicsInfo(&gpu_info_);
  switch (result) {
    case gpu::kCollectInfoFatalFailure:
      LOG(ERROR) << "gpu::CollectGraphicsInfo failed (fatal).";
      // TODO(piman): can we signal overall failure?
      break;
    case gpu::kCollectInfoNonFatalFailure:
      DVLOG(1) << "gpu::CollectGraphicsInfo failed (non-fatal).";
      break;
    case gpu::kCollectInfoNone:
      NOTREACHED();
      break;
    case gpu::kCollectInfoSuccess:
      break;
  }
  gpu::SetKeysForCrashLogging(gpu_info_);
}
#elif defined(OS_WIN)
void GpuService::UpdateGpuInfoPlatform() {
  // GPU full info collection should only happen on un-sandboxed GPU process
  // or single process/in-process gpu mode on Windows.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  DCHECK(command_line->HasSwitch("disable-gpu-sandbox") || in_host_process_);

  // This is slow, but it's the only thing the unsandboxed GPU process does,
  // and GpuDataManager prevents us from sending multiple collecting requests,
  // so it's OK to be blocking.
  gpu::GetDxDiagnostics(&gpu_info_.dx_diagnostics);
  gpu_info_.dx_diagnostics_info_state = gpu::kCollectInfoSuccess;
}
#else
void GpuService::UpdateGpuInfoPlatform() {}
#endif

void GpuService::DidCreateOffscreenContext(const GURL& active_url) {
  (*gpu_host_)->DidCreateOffscreenContext(active_url);
}

void GpuService::DidDestroyChannel(int client_id) {
  media_gpu_channel_manager_->RemoveChannel(client_id);
  (*gpu_host_)->DidDestroyChannel(client_id);
}

void GpuService::DidDestroyOffscreenContext(const GURL& active_url) {
  (*gpu_host_)->DidDestroyOffscreenContext(active_url);
}

void GpuService::DidLoseContext(bool offscreen,
                                gpu::error::ContextLostReason reason,
                                const GURL& active_url) {
  (*gpu_host_)->DidLoseContext(offscreen, reason, active_url);
}

void GpuService::StoreShaderToDisk(int client_id,
                                   const std::string& key,
                                   const std::string& shader) {
  (*gpu_host_)->StoreShaderToDisk(client_id, key, shader);
}

#if defined(OS_WIN)
void GpuService::SendAcceleratedSurfaceCreatedChildWindow(
    gpu::SurfaceHandle parent_window,
    gpu::SurfaceHandle child_window) {
  (*gpu_host_)->SetChildSurface(parent_window, child_window);
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

void GpuService::CloseChannel(int32_t client_id) {
  if (!gpu_channel_manager_)
    return;
  gpu_channel_manager_->RemoveChannel(client_id);
}

void GpuService::LoadedShader(const std::string& data) {
  if (!gpu_channel_manager_)
    return;
  gpu_channel_manager_->PopulateShaderCache(data);
}

void GpuService::DestroyingVideoSurface(
    int32_t surface_id,
    const DestroyingVideoSurfaceCallback& callback) {
#if defined(OS_ANDROID)
  media::AVDACodecAllocator::Instance()->OnSurfaceDestroyed(surface_id);
#else
  NOTREACHED() << "DestroyingVideoSurface() not supported on this platform.";
#endif
  callback.Run();
}

void GpuService::WakeUpGpu() {
#if defined(OS_ANDROID)
  if (!gpu_channel_manager_)
    return;
  gpu_channel_manager_->WakeUpGpu();
#else
  NOTREACHED() << "WakeUpGpu() not supported on this platform.";
#endif
}

void GpuService::DestroyAllChannels() {
  if (!gpu_channel_manager_)
    return;
  DVLOG(1) << "GPU: Removing all contexts";
  gpu_channel_manager_->DestroyAllChannels();
}

void GpuService::Crash() {
  DVLOG(1) << "GPU: Simulating GPU crash";
  // Good bye, cruel world.
  volatile int* it_s_the_end_of_the_world_as_we_know_it = NULL;
  *it_s_the_end_of_the_world_as_we_know_it = 0xdead;
}

void GpuService::Hang() {
  DVLOG(1) << "GPU: Simulating GPU hang";
  for (;;) {
    // Do not sleep here. The GPU watchdog timer tracks the amount of user
    // time this thread is using and it doesn't use much while calling Sleep.
  }
}

void GpuService::ThrowJavaException() {
#if defined(OS_ANDROID)
  base::android::ThrowUncaughtException();
#else
  NOTREACHED() << "Java exception not supported on this platform.";
#endif
}

void GpuService::Stop(const StopCallback& callback) {
  base::MessageLoop::current()->QuitWhenIdle();
  callback.Run();
}

}  // namespace ui
