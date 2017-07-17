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
#include "components/viz/common/gpu/in_process_context_provider.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/common/memory_stats.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"
#include "media/gpu/ipc/service/gpu_jpeg_decode_accelerator_factory_provider.h"
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
#include "media/gpu/content_video_view_overlay_allocator.h"
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

// Returns a callback which does a PostTask to run |callback| on the |runner|
// task runner.
template <typename Param>
const base::Callback<void(const Param&)> WrapCallback(
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const base::Callback<void(const Param&)>& callback) {
  return base::Bind(
      [](scoped_refptr<base::SingleThreadTaskRunner> runner,
         const base::Callback<void(const Param&)>& callback,
         const Param& param) {
        runner->PostTask(FROM_HERE, base::Bind(callback, param));
      },
      runner, callback);
}

void DestroyBinding(mojo::BindingSet<mojom::GpuService>* binding,
                    base::WaitableEvent* wait) {
  binding->CloseAllBindings();
  wait->Signal();
}

}  // namespace

GpuService::GpuService(const gpu::GPUInfo& gpu_info,
                       std::unique_ptr<gpu::GpuWatchdogThread> watchdog_thread,
                       scoped_refptr<base::SingleThreadTaskRunner> io_runner,
                       const gpu::GpuFeatureInfo& gpu_feature_info)
    : main_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_runner_(std::move(io_runner)),
      watchdog_thread_(std::move(watchdog_thread)),
      gpu_memory_buffer_factory_(
          gpu::GpuMemoryBufferFactory::CreateNativeType()),
      gpu_workarounds_(base::CommandLine::ForCurrentProcess()),
      gpu_info_(gpu_info),
      gpu_feature_info_(gpu_feature_info),
      bindings_(base::MakeUnique<mojo::BindingSet<mojom::GpuService>>()),
      weak_ptr_factory_(this) {
  DCHECK(!io_runner_->BelongsToCurrentThread());
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

GpuService::~GpuService() {
  DCHECK(main_runner_->BelongsToCurrentThread());
  bind_task_tracker_.TryCancelAll();
  logging::SetLogMessageHandler(nullptr);
  g_log_callback.Get() =
      base::Callback<void(int, size_t, const std::string&)>();
  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  if (io_runner_->PostTask(
          FROM_HERE, base::Bind(&DestroyBinding, bindings_.get(), &wait))) {
    wait.Wait();
  }
  media_gpu_channel_manager_.reset();
  gpu_channel_manager_.reset();
  owned_sync_point_manager_.reset();

  // Signal this event before destroying the child process. That way all
  // background threads can cleanup. For example, in the renderer the
  // RenderThread instances will be able to notice shutdown before the render
  // process begins waiting for them to exit.
  if (owned_shutdown_event_)
    owned_shutdown_event_->Signal();
}

void GpuService::UpdateGPUInfoFromPreferences(
    const gpu::GpuPreferences& preferences) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  DCHECK(!gpu_host_);
  gpu_preferences_ = preferences;
  gpu_info_.video_decode_accelerator_capabilities =
      media::GpuVideoDecodeAccelerator::GetCapabilities(gpu_preferences_,
                                                        gpu_workarounds_);
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
                                    gpu::GpuProcessActivityFlags activity_flags,
                                    gpu::SyncPointManager* sync_point_manager,
                                    base::WaitableEvent* shutdown_event) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  gpu_host->DidInitialize(gpu_info_, gpu_feature_info_);
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

  shutdown_event_ = shutdown_event;
  if (!shutdown_event_) {
    owned_shutdown_event_ = base::MakeUnique<base::WaitableEvent>(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    shutdown_event_ = owned_shutdown_event_.get();
  }

  if (gpu_preferences_.enable_gpu_scheduler) {
    scheduler_ = base::MakeUnique<gpu::Scheduler>(
        base::ThreadTaskRunnerHandle::Get(), sync_point_manager_);
  }

  // Defer creation of the render thread. This is to prevent it from handling
  // IPC messages before the sandbox has been enabled and all other necessary
  // initialization has succeeded.
  gpu_channel_manager_.reset(new gpu::GpuChannelManager(
      gpu_preferences_, gpu_workarounds_, this, watchdog_thread_.get(),
      main_runner_, io_runner_, scheduler_.get(), sync_point_manager_,
      gpu_memory_buffer_factory_.get(), gpu_feature_info_,
      std::move(activity_flags)));

  media_gpu_channel_manager_.reset(
      new media::MediaGpuChannelManager(gpu_channel_manager_.get()));
  if (watchdog_thread())
    watchdog_thread()->AddPowerObserver();
}

void GpuService::Bind(mojom::GpuServiceRequest request) {
  if (main_runner_->BelongsToCurrentThread()) {
    bind_task_tracker_.PostTask(
        io_runner_.get(), FROM_HERE,
        base::Bind(&GpuService::Bind, base::Unretained(this),
                   base::Passed(std::move(request))));
    return;
  }
  bindings_->AddBinding(this, std::move(request));
}

gpu::ImageFactory* GpuService::gpu_image_factory() {
  return gpu_memory_buffer_factory_
             ? gpu_memory_buffer_factory_->AsImageFactory()
             : nullptr;
}

void GpuService::RecordLogMessage(int severity,
                                  size_t message_start,
                                  const std::string& str) {
  // This can be run from any thread.
  std::string header = str.substr(0, message_start);
  std::string message = str.substr(message_start);
  (*gpu_host_)->RecordLogMessage(severity, header, message);
}

void GpuService::CreateJpegDecodeAccelerator(
    media::mojom::GpuJpegDecodeAcceleratorRequest jda_request) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  // TODO(c.padhi): Implement this, see https://crbug.com/699255.
  NOTIMPLEMENTED();
}

void GpuService::CreateVideoEncodeAccelerator(
    media::mojom::VideoEncodeAcceleratorRequest vea_request) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  // TODO(mcasas): Create a mojom::VideoEncodeAccelerator implementation,
  // https://crbug.com/736517.
}

void GpuService::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    gpu::SurfaceHandle surface_handle,
    const CreateGpuMemoryBufferCallback& callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  // This needs to happen in the IO thread.
  callback.Run(gpu_memory_buffer_factory_->CreateGpuMemoryBuffer(
      id, size, format, usage, client_id, surface_handle));
}

void GpuService::DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                        int client_id,
                                        const gpu::SyncToken& sync_token) {
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::Bind(&GpuService::DestroyGpuMemoryBuffer, weak_ptr_,
                              id, client_id, sync_token));
    return;
  }
  gpu_channel_manager_->DestroyGpuMemoryBuffer(id, client_id, sync_token);
}

void GpuService::GetVideoMemoryUsageStats(
    const GetVideoMemoryUsageStatsCallback& callback) {
  if (io_runner_->BelongsToCurrentThread()) {
    auto wrap_callback = WrapCallback(io_runner_, callback);
    main_runner_->PostTask(
        FROM_HERE, base::Bind(&GpuService::GetVideoMemoryUsageStats, weak_ptr_,
                              wrap_callback));
    return;
  }
  gpu::VideoMemoryUsageStats video_memory_usage_stats;
  gpu_channel_manager_->gpu_memory_manager()->GetVideoMemoryUsageStats(
      &video_memory_usage_stats);
  callback.Run(video_memory_usage_stats);
}

void GpuService::RequestCompleteGpuInfo(
    const RequestCompleteGpuInfoCallback& callback) {
  if (io_runner_->BelongsToCurrentThread()) {
    auto wrap_callback = WrapCallback(io_runner_, callback);
    main_runner_->PostTask(
        FROM_HERE, base::Bind(&GpuService::RequestCompleteGpuInfo, weak_ptr_,
                              wrap_callback));
    return;
  }
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
  DCHECK(main_runner_->BelongsToCurrentThread());
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
  DCHECK(main_runner_->BelongsToCurrentThread());
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
  DCHECK(main_runner_->BelongsToCurrentThread());
  (*gpu_host_)->DidCreateOffscreenContext(active_url);
}

void GpuService::DidDestroyChannel(int client_id) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  media_gpu_channel_manager_->RemoveChannel(client_id);
  (*gpu_host_)->DidDestroyChannel(client_id);
}

void GpuService::DidDestroyOffscreenContext(const GURL& active_url) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  (*gpu_host_)->DidDestroyOffscreenContext(active_url);
}

void GpuService::DidLoseContext(bool offscreen,
                                gpu::error::ContextLostReason reason,
                                const GURL& active_url) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  (*gpu_host_)->DidLoseContext(offscreen, reason, active_url);
}

void GpuService::StoreShaderToDisk(int client_id,
                                   const std::string& key,
                                   const std::string& shader) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  (*gpu_host_)->StoreShaderToDisk(client_id, key, shader);
}

#if defined(OS_WIN)
void GpuService::SendAcceleratedSurfaceCreatedChildWindow(
    gpu::SurfaceHandle parent_window,
    gpu::SurfaceHandle child_window) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  (*gpu_host_)->SetChildSurface(parent_window, child_window);
}
#endif

void GpuService::SetActiveURL(const GURL& url) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  constexpr char kActiveURL[] = "url-chunk";
  base::debug::SetCrashKeyValue(kActiveURL, url.possibly_invalid_spec());
}

void GpuService::EstablishGpuChannel(
    int32_t client_id,
    uint64_t client_tracing_id,
    bool is_gpu_host,
    const EstablishGpuChannelCallback& callback) {
  if (io_runner_->BelongsToCurrentThread()) {
    EstablishGpuChannelCallback wrap_callback = base::Bind(
        [](scoped_refptr<base::SingleThreadTaskRunner> runner,
           const EstablishGpuChannelCallback& cb,
           mojo::ScopedMessagePipeHandle handle) {
          runner->PostTask(FROM_HERE,
                           base::Bind(cb, base::Passed(std::move(handle))));
        },
        io_runner_, callback);
    main_runner_->PostTask(
        FROM_HERE,
        base::Bind(&GpuService::EstablishGpuChannel, weak_ptr_, client_id,
                   client_tracing_id, is_gpu_host, wrap_callback));
    return;
  }

  gpu::GpuChannel* gpu_channel = gpu_channel_manager_->EstablishChannel(
      client_id, client_tracing_id, is_gpu_host);

  mojo::MessagePipe pipe;
  gpu_channel->Init(base::MakeUnique<gpu::SyncChannelFilteredSender>(
      pipe.handle0.release(), gpu_channel, io_runner_, shutdown_event_));

  media_gpu_channel_manager_->AddChannel(client_id);

  callback.Run(std::move(pipe.handle1));
}

void GpuService::CloseChannel(int32_t client_id) {
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::Bind(&GpuService::CloseChannel, weak_ptr_, client_id));
    return;
  }
  gpu_channel_manager_->RemoveChannel(client_id);
}

void GpuService::LoadedShader(const std::string& key, const std::string& data) {
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::Bind(&GpuService::LoadedShader, weak_ptr_, key, data));
    return;
  }
  gpu_channel_manager_->PopulateShaderCache(key, data);
}

void GpuService::DestroyingVideoSurface(
    int32_t surface_id,
    const DestroyingVideoSurfaceCallback& callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());
#if defined(OS_ANDROID)
  main_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(
          [](int32_t surface_id) {
            media::ContentVideoViewOverlayAllocator::GetInstance()
                ->OnSurfaceDestroyed(surface_id);
          },
          surface_id),
      callback);
#else
  NOTREACHED() << "DestroyingVideoSurface() not supported on this platform.";
#endif
}

void GpuService::WakeUpGpu() {
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(FROM_HERE,
                           base::Bind(&GpuService::WakeUpGpu, weak_ptr_));
    return;
  }
#if defined(OS_ANDROID)
  gpu_channel_manager_->WakeUpGpu();
#else
  NOTREACHED() << "WakeUpGpu() not supported on this platform.";
#endif
}

void GpuService::GpuSwitched() {
  DVLOG(1) << "GPU: GPU has switched";
  if (!in_host_process_)
    ui::GpuSwitchingManager::GetInstance()->NotifyGpuSwitched();
}

void GpuService::DestroyAllChannels() {
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::Bind(&GpuService::DestroyAllChannels, weak_ptr_));
    return;
  }
  DVLOG(1) << "GPU: Removing all contexts";
  gpu_channel_manager_->DestroyAllChannels();
}

void GpuService::Crash() {
  DCHECK(io_runner_->BelongsToCurrentThread());
  DVLOG(1) << "GPU: Simulating GPU crash";
  // Good bye, cruel world.
  volatile int* it_s_the_end_of_the_world_as_we_know_it = NULL;
  *it_s_the_end_of_the_world_as_we_know_it = 0xdead;
}

void GpuService::Hang() {
  DCHECK(io_runner_->BelongsToCurrentThread());

  main_runner_->PostTask(FROM_HERE, base::Bind([] {
                           DVLOG(1) << "GPU: Simulating GPU hang";
                           for (;;) {
                             // Do not sleep here. The GPU watchdog timer tracks
                             // the amount of user time this thread is using and
                             // it doesn't use much while calling Sleep.
                           }
                         }));
}

void GpuService::ThrowJavaException() {
  DCHECK(io_runner_->BelongsToCurrentThread());
#if defined(OS_ANDROID)
  main_runner_->PostTask(
      FROM_HERE, base::Bind([] { base::android::ThrowUncaughtException(); }));
#else
  NOTREACHED() << "Java exception not supported on this platform.";
#endif
}

void GpuService::Stop(const StopCallback& callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTaskAndReply(FROM_HERE, base::Bind([] {
                                   base::MessageLoop::current()->QuitWhenIdle();
                                 }),
                                 callback);
}

}  // namespace ui
