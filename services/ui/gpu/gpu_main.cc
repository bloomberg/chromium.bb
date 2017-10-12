// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/gpu/gpu_main.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/service/display_embedder/gpu_display_provider.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(OS_MACOSX)
#include "base/message_loop/message_pump_mac.h"
#endif

namespace {

#if defined(USE_X11)
std::unique_ptr<base::MessagePump> CreateMessagePumpX11() {
  // TODO(sad): This should create a TYPE_UI message pump, and create a
  // PlatformEventSource when gpu process split happens.
  return base::MessageLoop::CreateMessagePumpForType(
      base::MessageLoop::TYPE_DEFAULT);
}
#endif  // defined(USE_X11)

#if defined(OS_MACOSX)
std::unique_ptr<base::MessagePump> CreateMessagePumpMac() {
  return base::MakeUnique<base::MessagePumpCFRunLoop>();
}
#endif  // defined(OS_MACOSX)

std::unique_ptr<base::Thread> CreateAndStartCompositorThread() {
  auto thread = std::make_unique<base::Thread>("CompositorThread");
  base::Thread::Options thread_options;
#if defined(OS_WIN)
  thread_options.message_loop_type = base::MessageLoop::TYPE_DEFAULT;
#elif defined(USE_X11)
  thread_options.message_pump_factory = base::Bind(&CreateMessagePumpX11);
#elif defined(USE_OZONE)
  // The MessageLoop type required depends on the Ozone platform selected at
  // runtime.
  thread_options.message_loop_type =
      ui::OzonePlatform::EnsureInstance()->GetMessageLoopTypeForGpu();
#elif defined(OS_LINUX)
  thread_options.message_loop_type = base::MessageLoop::TYPE_DEFAULT;
#elif defined(OS_MACOSX)
  thread_options.message_pump_factory = base::Bind(&CreateMessagePumpMac);
#else
  thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
#endif

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  thread_options.priority = base::ThreadPriority::DISPLAY;
#endif
  CHECK(thread->StartWithOptions(thread_options));
  return thread;
}

std::unique_ptr<base::Thread> CreateAndStartIOThread() {
  // TODO(sad): We do not need the IO thread once gpu has a separate process.
  // It should be possible to use |main_task_runner_| for doing IO tasks.
  base::Thread::Options thread_options(base::MessageLoop::TYPE_IO, 0);
  thread_options.priority = base::ThreadPriority::NORMAL;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  // TODO(reveman): Remove this in favor of setting it explicitly for each
  // type of process.
  thread_options.priority = base::ThreadPriority::DISPLAY;
#endif
  auto io_thread = std::make_unique<base::Thread>("GpuIOThread");
  CHECK(io_thread->StartWithOptions(thread_options));
  return io_thread;
}

}  // namespace

namespace ui {

GpuMain::ExternalDependencies::ExternalDependencies() = default;

GpuMain::ExternalDependencies::~ExternalDependencies() = default;

GpuMain::ExternalDependencies::ExternalDependencies(
    ExternalDependencies&& other) = default;

GpuMain::ExternalDependencies& GpuMain::ExternalDependencies::operator=(
    ExternalDependencies&& other) = default;

GpuMain::GpuMain(Delegate* delegate,
                 ExternalDependencies dependencies,
                 std::unique_ptr<gpu::GpuInit> gpu_init)
    : delegate_(delegate),
      dependencies_(std::move(dependencies)),
      gpu_init_(std::move(gpu_init)),
      gpu_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      binding_(this),
      associated_binding_(this) {
  // TODO(crbug.com/609317): Remove this when Mus Window Server and GPU are
  // split into separate processes. Until then this is necessary to be able to
  // run Mushrome (chrome --mus) with Mus running in the browser process.
  if (!base::PowerMonitor::Get()) {
    power_monitor_ = base::MakeUnique<base::PowerMonitor>(
        base::MakeUnique<base::PowerMonitorDeviceSource>());
  }

  if (!gpu_init_) {
    // Initialize GpuInit before starting the IO or compositor threads.
    gpu_init_ = std::make_unique<gpu::GpuInit>();
    gpu_init_->set_sandbox_helper(this);
    // TODO(crbug.com/609317): Use InitializeAndStartSandbox() when gpu-mus is
    // split into a separate process.
    gpu_init_->InitializeInProcess(base::CommandLine::ForCurrentProcess());
  }

  if (!dependencies_.io_thread_task_runner)
    io_thread_ = CreateAndStartIOThread();
  if (dependencies_.create_display_compositor) {
    compositor_thread_ = CreateAndStartCompositorThread();
    compositor_thread_task_runner_ = compositor_thread_->task_runner();
  }

  gpu_service_ = base::MakeUnique<viz::GpuServiceImpl>(
      gpu_init_->gpu_info(), gpu_init_->TakeWatchdogThread(),
      io_thread_ ? io_thread_->task_runner()
                 : dependencies_.io_thread_task_runner,
      gpu_init_->gpu_feature_info());
}

GpuMain::~GpuMain() {
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());
  if (io_thread_)
    io_thread_->Stop();
}

void GpuMain::SetLogMessagesForHost(LogMessages log_messages) {
  log_messages_ = std::move(log_messages);
}

void GpuMain::Bind(mojom::GpuMainRequest request) {
  binding_.Bind(std::move(request));
}

void GpuMain::BindAssociated(mojom::GpuMainAssociatedRequest request) {
  associated_binding_.Bind(std::move(request));
}

void GpuMain::TearDown() {
  DCHECK(!gpu_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(!compositor_thread_task_runner_->BelongsToCurrentThread());
  // The compositor holds on to some resources from gpu service. So destroy the
  // compositor first, before destroying the gpu service. However, before the
  // compositor is destroyed, close the binding, so that the gpu service doesn't
  // need to process commands from the compositor as it is shutting down.
  base::WaitableEvent binding_wait(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  gpu_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&GpuMain::CloseGpuMainBindingOnGpuThread,
                            base::Unretained(this), &binding_wait));
  binding_wait.Wait();

  base::WaitableEvent compositor_wait(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  compositor_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&GpuMain::TearDownOnCompositorThread,
                            base::Unretained(this), &compositor_wait));
  compositor_wait.Wait();

  base::WaitableEvent gpu_wait(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
  gpu_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&GpuMain::TearDownOnGpuThread,
                            base::Unretained(this), &gpu_wait));
  gpu_wait.Wait();
}

void GpuMain::CreateGpuService(viz::mojom::GpuServiceRequest request,
                               mojom::GpuHostPtr gpu_host,
                               const gpu::GpuPreferences& preferences,
                               mojo::ScopedSharedBufferHandle activity_flags) {
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());
  gpu_service_->UpdateGPUInfoFromPreferences(preferences);
  for (const LogMessage& log : log_messages_)
    gpu_host->RecordLogMessage(log.severity, log.header, log.message);
  log_messages_.clear();
  if (!gpu_init_->init_successful()) {
    LOG(ERROR) << "Exiting GPU process due to errors during initialization";
    gpu_service_.reset();
    gpu_host->DidFailInitialize();
    if (delegate_)
      delegate_->OnInitializationFailed();
    return;
  }

  gpu_service_->Bind(std::move(request));
  gpu_service_->InitializeWithHost(
      std::move(gpu_host),
      gpu::GpuProcessActivityFlags(std::move(activity_flags)),
      dependencies_.sync_point_manager, dependencies_.shutdown_event);

  if (pending_frame_sink_manager_request_.is_pending()) {
    CreateFrameSinkManagerInternal(
        std::move(pending_frame_sink_manager_request_),
        std::move(pending_frame_sink_manager_client_info_));
  }
  if (delegate_)
    delegate_->OnGpuServiceConnection(gpu_service_.get());
}

void GpuMain::CreateFrameSinkManager(
    viz::mojom::FrameSinkManagerRequest request,
    viz::mojom::FrameSinkManagerClientPtr client) {
  DCHECK(compositor_thread_task_runner_);
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());
  if (!gpu_service_ || !gpu_service_->is_initialized()) {
    pending_frame_sink_manager_request_ = std::move(request);
    pending_frame_sink_manager_client_info_ = client.PassInterface();
    return;
  }
  CreateFrameSinkManagerInternal(std::move(request), client.PassInterface());
}

void GpuMain::CreateFrameSinkManagerInternal(
    viz::mojom::FrameSinkManagerRequest request,
    viz::mojom::FrameSinkManagerClientPtrInfo client_info) {
  DCHECK(!gpu_command_service_);
  DCHECK(gpu_service_);
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());
  gpu_command_service_ = new gpu::GpuInProcessThreadService(
      gpu_thread_task_runner_, gpu_service_->sync_point_manager(),
      gpu_service_->mailbox_manager(), gpu_service_->share_group(),
      gpu_service_->gpu_feature_info());

  compositor_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GpuMain::CreateFrameSinkManagerOnCompositorThread,
                 base::Unretained(this), base::Passed(std::move(request)),
                 base::Passed(std::move(client_info))));
}

void GpuMain::CreateFrameSinkManagerOnCompositorThread(
    viz::mojom::FrameSinkManagerRequest request,
    viz::mojom::FrameSinkManagerClientPtrInfo client_info) {
  DCHECK(!frame_sink_manager_);
  viz::mojom::FrameSinkManagerClientPtr client;
  client.Bind(std::move(client_info));

  display_provider_ = base::MakeUnique<viz::GpuDisplayProvider>(
      gpu_command_service_, gpu_service_->gpu_channel_manager());

  frame_sink_manager_ = base::MakeUnique<viz::FrameSinkManagerImpl>(
      viz::SurfaceManager::LifetimeType::REFERENCES, display_provider_.get());
  frame_sink_manager_->BindAndSetClient(std::move(request), nullptr,
                                        std::move(client));
}

void GpuMain::CloseGpuMainBindingOnGpuThread(base::WaitableEvent* wait) {
  binding_.Close();
  wait->Signal();
}

void GpuMain::TearDownOnCompositorThread(base::WaitableEvent* wait) {
  frame_sink_manager_.reset();
  display_provider_.reset();
  wait->Signal();
}

void GpuMain::TearDownOnGpuThread(base::WaitableEvent* wait) {
  gpu_command_service_ = nullptr;
  gpu_service_.reset();
  gpu_memory_buffer_factory_.reset();
  gpu_init_.reset();
  wait->Signal();
}

void GpuMain::PreSandboxStartup() {
  // TODO(sad): https://crbug.com/645602
}

bool GpuMain::EnsureSandboxInitialized(gpu::GpuWatchdogThread* watchdog_thread,
                                       const gpu::GPUInfo* gpu_info) {
  // TODO(sad): https://crbug.com/645602
  return true;
}

}  // namespace ui
