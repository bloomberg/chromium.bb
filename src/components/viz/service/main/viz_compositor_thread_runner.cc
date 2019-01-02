// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/main/viz_compositor_thread_runner.h"

#include <utility>

#include "base/command_line.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/switches.h"
#include "components/viz/service/display_embedder/gpu_display_provider.h"
#include "components/viz/service/display_embedder/in_process_gpu_memory_buffer_manager.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/ipc/command_buffer_task_executor.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "ui/gfx/switches.h"

namespace viz {
namespace {

const char kThreadName[] = "VizCompositorThread";

std::unique_ptr<VizCompositorThreadType> CreateAndStartCompositorThread() {
#if defined(OS_ANDROID)
  auto thread = std::make_unique<base::android::JavaHandlerThread>(
      kThreadName, base::ThreadPriority::DISPLAY);
  thread->Start();
  return thread;
#else  // !defined(OS_ANDROID)
  auto thread = std::make_unique<base::Thread>(kThreadName);

  base::Thread::Options thread_options;
#if defined(OS_WIN)
  // Windows needs a UI message loop for child HWND. Other platforms can use the
  // default message loop type.
  thread_options.message_loop_type = base::MessageLoop::TYPE_UI;
#elif defined(OS_CHROMEOS)
  thread_options.priority = base::ThreadPriority::DISPLAY;
#endif

  CHECK(thread->StartWithOptions(thread_options));
  return thread;
#endif  // !defined(OS_ANDROID)
}

}  // namespace

VizCompositorThreadRunner::VizCompositorThreadRunner()
    : thread_(CreateAndStartCompositorThread()),
      task_runner_(thread_->task_runner()) {}

VizCompositorThreadRunner::~VizCompositorThreadRunner() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VizCompositorThreadRunner::TearDownOnCompositorThread,
                     base::Unretained(this)));
  thread_->Stop();
}

void VizCompositorThreadRunner::CreateFrameSinkManager(
    mojom::FrameSinkManagerParamsPtr params) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VizCompositorThreadRunner::CreateFrameSinkManagerOnCompositorThread,
          base::Unretained(this), std::move(params), nullptr, nullptr, nullptr,
          nullptr));
}

void VizCompositorThreadRunner::CreateFrameSinkManager(
    mojom::FrameSinkManagerParamsPtr params,
    scoped_refptr<gpu::CommandBufferTaskExecutor> task_executor,
    GpuServiceImpl* gpu_service) {
  auto* gpu_channel_manager = gpu_service->gpu_channel_manager();
  auto* image_factory = gpu_service->gpu_image_factory();

  // All of the unretained objects are owned on the GPU thread and destroyed
  // after VizCompositorThread has been shutdown.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VizCompositorThreadRunner::CreateFrameSinkManagerOnCompositorThread,
          base::Unretained(this), std::move(params), std::move(task_executor),
          base::Unretained(gpu_service), base::Unretained(image_factory),
          base::Unretained(gpu_channel_manager)));
}

void VizCompositorThreadRunner::CleanupForShutdown(
    base::OnceClosure cleanup_finished_callback) {
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &VizCompositorThreadRunner::CleanupForShutdownOnCompositorThread,
          base::Unretained(this)),
      std::move(cleanup_finished_callback));
}

void VizCompositorThreadRunner::CreateFrameSinkManagerOnCompositorThread(
    mojom::FrameSinkManagerParamsPtr params,
    scoped_refptr<gpu::CommandBufferTaskExecutor> task_executor,
    GpuServiceImpl* gpu_service,
    gpu::ImageFactory* image_factory,
    gpu::GpuChannelManager* gpu_channel_manager) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!frame_sink_manager_);

  server_shared_bitmap_manager_ = std::make_unique<ServerSharedBitmapManager>();
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      server_shared_bitmap_manager_.get(), "ServerSharedBitmapManager",
      task_runner_);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const bool headless = command_line->HasSwitch(switches::kHeadless);
  const bool run_all_compositor_stages_before_draw =
      command_line->HasSwitch(switches::kRunAllCompositorStagesBeforeDraw);

  if (task_executor) {
    // Create DisplayProvider usable for GPU + software compositing.
    display_provider_ = std::make_unique<GpuDisplayProvider>(
        params->restart_id, gpu_service, std::move(task_executor), gpu_service,
        std::make_unique<InProcessGpuMemoryBufferManager>(gpu_channel_manager),
        image_factory, server_shared_bitmap_manager_.get(), headless,
        run_all_compositor_stages_before_draw);
  } else {
    // Create DisplayProvider usable for software compositing only.
    display_provider_ = std::make_unique<GpuDisplayProvider>(
        params->restart_id, server_shared_bitmap_manager_.get(), headless,
        run_all_compositor_stages_before_draw);
  }

  // Create FrameSinkManagerImpl.
  base::Optional<uint32_t> activation_deadline_in_frames;
  if (params->use_activation_deadline)
    activation_deadline_in_frames = params->activation_deadline_in_frames;
  frame_sink_manager_ = std::make_unique<FrameSinkManagerImpl>(
      server_shared_bitmap_manager_.get(), activation_deadline_in_frames,
      display_provider_.get());
  frame_sink_manager_->BindAndSetClient(
      std::move(params->frame_sink_manager), nullptr,
      mojom::FrameSinkManagerClientPtr(
          std::move(params->frame_sink_manager_client)));
}

void VizCompositorThreadRunner::CleanupForShutdownOnCompositorThread() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (frame_sink_manager_)
    frame_sink_manager_->ForceShutdown();
}

void VizCompositorThreadRunner::TearDownOnCompositorThread() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (server_shared_bitmap_manager_) {
    base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
        server_shared_bitmap_manager_.get());
  }

  frame_sink_manager_.reset();
  display_provider_.reset();
  server_shared_bitmap_manager_.reset();
}

}  // namespace viz
