// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/gpu/gpu_main.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/single_thread_task_runner.h"
#include "components/viz/service/display_compositor/gpu_display_provider.h"
#include "components/viz/service/frame_sinks/mojo_frame_sink_manager.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "services/ui/gpu/gpu_service.h"

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

}  // namespace

namespace ui {

GpuMain::GpuMain(mojom::GpuMainRequest request)
    : gpu_thread_("GpuThread"),
      io_thread_("GpuIOThread"),
      compositor_thread_("DisplayCompositorThread"),
      power_monitor_(base::MakeUnique<base::PowerMonitorDeviceSource>()),
      binding_(this) {
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
  CHECK(gpu_thread_.StartWithOptions(thread_options));
  gpu_thread_task_runner_ = gpu_thread_.task_runner();

  // TODO(sad): We do not need the IO thread once gpu has a separate process. It
  // should be possible to use |main_task_runner_| for doing IO tasks.
  thread_options = base::Thread::Options(base::MessageLoop::TYPE_IO, 0);
  thread_options.priority = base::ThreadPriority::NORMAL;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  // TODO(reveman): Remove this in favor of setting it explicitly for each type
  // of process.
  thread_options.priority = base::ThreadPriority::DISPLAY;
#endif
  CHECK(io_thread_.StartWithOptions(thread_options));

  // Start the compositor thread.
  compositor_thread_.Start();
  compositor_thread_task_runner_ = compositor_thread_.task_runner();

  // |this| will outlive the gpu thread and so it's safe to use
  // base::Unretained here.
  gpu_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GpuMain::InitOnGpuThread, base::Unretained(this),
                 io_thread_.task_runner(), compositor_thread_task_runner_));
  gpu_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&GpuMain::BindOnGpu, base::Unretained(this),
                            base::Passed(std::move(request))));
}

GpuMain::~GpuMain() {
  // Unretained() is OK here since the thread/task runner is owned by |this|.
  compositor_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GpuMain::TearDownOnCompositorThread, base::Unretained(this)));

  // Block the main thread until the compositor thread terminates which blocks
  // on the gpu thread. The Stop must be initiated from here instead of the gpu
  // thread to avoid deadlock.
  compositor_thread_.Stop();

  gpu_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GpuMain::TearDownOnGpuThread, base::Unretained(this)));
  gpu_thread_.Stop();
  io_thread_.Stop();
}

void GpuMain::CreateGpuService(mojom::GpuServiceRequest request,
                               mojom::GpuHostPtr gpu_host,
                               const gpu::GpuPreferences& preferences,
                               mojo::ScopedSharedBufferHandle activity_flags) {
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());
  CreateGpuServiceOnGpuThread(
      std::move(request), std::move(gpu_host), preferences,
      gpu::GpuProcessActivityFlags(std::move(activity_flags)));
}

void GpuMain::CreateFrameSinkManager(
    cc::mojom::FrameSinkManagerRequest request,
    cc::mojom::FrameSinkManagerClientPtr client) {
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());
  if (!gpu_service_ || !gpu_service_->is_initialized()) {
    pending_frame_sink_manager_request_ = std::move(request);
    pending_frame_sink_manager_client_info_ = client.PassInterface();
    return;
  }
  CreateFrameSinkManagerInternal(std::move(request), client.PassInterface());
}

void GpuMain::BindOnGpu(mojom::GpuMainRequest request) {
  binding_.Bind(std::move(request));
}

void GpuMain::InitOnGpuThread(
    scoped_refptr<base::SingleThreadTaskRunner> io_runner,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_runner) {
  gpu_init_.reset(new gpu::GpuInit());
  gpu_init_->set_sandbox_helper(this);
  bool success = gpu_init_->InitializeAndStartSandbox(
      *base::CommandLine::ForCurrentProcess());
  if (!success)
    return;

  gpu_service_ = base::MakeUnique<GpuService>(
      gpu_init_->gpu_info(), gpu_init_->TakeWatchdogThread(), io_runner,
      gpu_init_->gpu_feature_info());
}

void GpuMain::CreateFrameSinkManagerInternal(
    cc::mojom::FrameSinkManagerRequest request,
    cc::mojom::FrameSinkManagerClientPtrInfo client_info) {
  DCHECK(!gpu_command_service_);
  DCHECK(gpu_service_);
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());
  gpu_command_service_ = new gpu::GpuInProcessThreadService(
      gpu_thread_task_runner_, gpu_service_->sync_point_manager(),
      gpu_service_->mailbox_manager(), gpu_service_->share_group());

  compositor_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GpuMain::CreateFrameSinkManagerOnCompositorThread,
                 base::Unretained(this), base::Passed(std::move(request)),
                 base::Passed(std::move(client_info))));
}

void GpuMain::CreateFrameSinkManagerOnCompositorThread(
    cc::mojom::FrameSinkManagerRequest request,
    cc::mojom::FrameSinkManagerClientPtrInfo client_info) {
  DCHECK(!frame_sink_manager_);
  cc::mojom::FrameSinkManagerClientPtr client;
  client.Bind(std::move(client_info));

  display_provider_ = base::MakeUnique<viz::GpuDisplayProvider>(
      gpu_command_service_, gpu_service_->gpu_channel_manager());

  frame_sink_manager_ = base::MakeUnique<viz::MojoFrameSinkManager>(
      true, display_provider_.get());
  frame_sink_manager_->BindPtrAndSetClient(std::move(request),
                                           std::move(client));
}

void GpuMain::TearDownOnCompositorThread() {
  frame_sink_manager_.reset();
  display_provider_.reset();
}

void GpuMain::TearDownOnGpuThread() {
  binding_.Close();
  gpu_service_.reset();
  gpu_memory_buffer_factory_.reset();
  gpu_init_.reset();
}

void GpuMain::CreateGpuServiceOnGpuThread(
    mojom::GpuServiceRequest request,
    mojom::GpuHostPtr gpu_host,
    const gpu::GpuPreferences& preferences,
    gpu::GpuProcessActivityFlags activity_flags) {
  gpu_service_->UpdateGPUInfoFromPreferences(preferences);
  gpu_service_->InitializeWithHost(std::move(gpu_host),
                                   std::move(activity_flags));
  gpu_service_->Bind(std::move(request));

  if (pending_frame_sink_manager_request_.is_pending()) {
    CreateFrameSinkManagerInternal(
        std::move(pending_frame_sink_manager_request_),
        std::move(pending_frame_sink_manager_client_info_));
  }
}

void GpuMain::PreSandboxStartup() {
  // TODO(sad): https://crbug.com/645602
}

bool GpuMain::EnsureSandboxInitialized(
    gpu::GpuWatchdogThread* watchdog_thread) {
  // TODO(sad): https://crbug.com/645602
  return true;
}

}  // namespace ui
