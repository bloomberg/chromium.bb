// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/main/viz_compositor_thread_runner.h"

#include <utility>

#include "base/bind.h"
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

#if defined(USE_VIZ_DEVTOOLS)
#include "components/ui_devtools/css_agent.h"
#include "components/ui_devtools/devtools_server.h"
#include "components/ui_devtools/viz/dom_agent_viz.h"
#include "components/ui_devtools/viz/overlay_agent_viz.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

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
#if defined(USE_OZONE)
  // We may need a non-default message loop type for the platform surface.
  thread_options.message_loop_type =
      ui::OzonePlatform::GetInstance()->GetMessageLoopTypeForGpu();
#endif
#if defined(OS_CHROMEOS) || defined(USE_OZONE)
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
          base::Unretained(this), std::move(params), nullptr, nullptr));
}

void VizCompositorThreadRunner::CreateFrameSinkManager(
    mojom::FrameSinkManagerParamsPtr params,
    gpu::CommandBufferTaskExecutor* task_executor,
    GpuServiceImpl* gpu_service) {
  // All of the unretained objects are owned on the GPU thread and destroyed
  // after VizCompositorThread has been shutdown.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VizCompositorThreadRunner::CreateFrameSinkManagerOnCompositorThread,
          base::Unretained(this), std::move(params),
          base::Unretained(task_executor), base::Unretained(gpu_service)));
}

#if defined(USE_VIZ_DEVTOOLS)
void VizCompositorThreadRunner::CreateVizDevTools(
    mojom::VizDevToolsParamsPtr params) {
  // It is safe to use Unretained(this) because |this| owns the |task_runner_|,
  // and will outlive it.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VizCompositorThreadRunner::CreateVizDevToolsOnCompositorThread,
          base::Unretained(this), std::move(params)));
}
#endif

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
    gpu::CommandBufferTaskExecutor* task_executor,
    GpuServiceImpl* gpu_service) {
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
    DCHECK(gpu_service);
    // Create DisplayProvider usable for GPU + software compositing.
    auto gpu_memory_buffer_manager =
        std::make_unique<InProcessGpuMemoryBufferManager>(
            gpu_service->gpu_memory_buffer_factory(),
            gpu_service->sync_point_manager());
    auto* image_factory = gpu_service->gpu_image_factory();
    display_provider_ = std::make_unique<GpuDisplayProvider>(
        params->restart_id, gpu_service, task_executor, gpu_service,
        std::move(gpu_memory_buffer_manager), image_factory,
        server_shared_bitmap_manager_.get(), headless,
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

#if defined(USE_VIZ_DEVTOOLS)
  if (pending_viz_dev_tools_params_)
    InitVizDevToolsOnCompositorThread(std::move(pending_viz_dev_tools_params_));
#endif
}

#if defined(USE_VIZ_DEVTOOLS)
void VizCompositorThreadRunner::CreateVizDevToolsOnCompositorThread(
    mojom::VizDevToolsParamsPtr params) {
  if (!frame_sink_manager_) {
    DCHECK(!pending_viz_dev_tools_params_);
    pending_viz_dev_tools_params_ = std::move(params);
    return;
  }
  InitVizDevToolsOnCompositorThread(std::move(params));
}

void VizCompositorThreadRunner::InitVizDevToolsOnCompositorThread(
    mojom::VizDevToolsParamsPtr params) {
  DCHECK(frame_sink_manager_);
  devtools_server_ = ui_devtools::UiDevToolsServer::CreateForViz(
      network::mojom::TCPServerSocketPtr(std::move(params->server_socket)),
      params->server_port);
  auto dom_agent =
      std::make_unique<ui_devtools::DOMAgentViz>(frame_sink_manager_.get());
  auto css_agent = std::make_unique<ui_devtools::CSSAgent>(dom_agent.get());
  auto overlay_agent =
      std::make_unique<ui_devtools::OverlayAgentViz>(dom_agent.get());
  auto devtools_client = std::make_unique<ui_devtools::UiDevToolsClient>(
      "VizDevToolsClient", devtools_server_.get());
  devtools_client->AddAgent(std::move(dom_agent));
  devtools_client->AddAgent(std::move(css_agent));
  devtools_client->AddAgent(std::move(overlay_agent));
  devtools_server_->AttachClient(std::move(devtools_client));
}
#endif

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

#if defined(USE_VIZ_DEVTOOLS)
  devtools_server_.reset();
#endif
  frame_sink_manager_.reset();
  display_provider_.reset();
  server_shared_bitmap_manager_.reset();
}

}  // namespace viz
