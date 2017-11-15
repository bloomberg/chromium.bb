// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/gpu_host.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/host/server_gpu_memory_buffer_manager.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl_shared_memory.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/ws/gpu_client.h"
#include "services/ui/ws/gpu_host_delegate.h"
#include "services/viz/public/interfaces/constants.mojom.h"
#include "ui/gfx/buffer_format_util.h"

#if defined(OS_WIN)
#include "ui/gfx/win/rendering_window_manager.h"
#endif

namespace ui {
namespace ws {

namespace {

// The client Id 1 is reserved for the frame sink manager.
const int32_t kInternalGpuChannelClientId = 2;

// TODO(crbug.com/620927): This should be removed once ozone-mojo is done.
bool HasSplitVizProcess() {
  constexpr char kEnableViz[] = "enable-viz";
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kEnableViz);
}

}  // namespace

DefaultGpuHost::DefaultGpuHost(GpuHostDelegate* delegate,
                               service_manager::Connector* connector)
    : delegate_(delegate),
      next_client_id_(kInternalGpuChannelClientId + 1),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      gpu_host_binding_(this),
      gpu_thread_("GpuThread"),
      viz_main_wait_(base::WaitableEvent::ResetPolicy::MANUAL,
                     base::WaitableEvent::InitialState::NOT_SIGNALED) {
  auto request = MakeRequest(&viz_main_);
  if (connector && HasSplitVizProcess()) {
    connector->BindInterface(viz::mojom::kVizServiceName, std::move(request));
  } else {
    // TODO(crbug.com/620927): This should be removed once ozone-mojo is done.
    gpu_thread_.Start();
    gpu_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&DefaultGpuHost::InitializeVizMain,
                                  base::Unretained(this),
                                  base::Passed(MakeRequest(&viz_main_))));
  }

  viz::mojom::GpuHostPtr gpu_host_proxy;
  gpu_host_binding_.Bind(mojo::MakeRequest(&gpu_host_proxy));
  viz_main_->CreateGpuService(MakeRequest(&gpu_service_),
                              std::move(gpu_host_proxy),
                              mojo::ScopedSharedBufferHandle());
  gpu_memory_buffer_manager_ =
      std::make_unique<viz::ServerGpuMemoryBufferManager>(gpu_service_.get(),
                                                          next_client_id_++);
}

DefaultGpuHost::~DefaultGpuHost() {
  // TODO(crbug.com/620927): This should be removed once ozone-mojo is done.
  // Make sure |viz_main_impl_| has been successfully created (i.e. the task
  // posted in the constructor to run InitializeVizMain() has actually run).
  if (gpu_thread_.IsRunning()) {
    viz_main_wait_.Wait();
    viz_main_impl_->TearDown();
    gpu_thread_.task_runner()->DeleteSoon(FROM_HERE, std::move(viz_main_impl_));
    gpu_thread_.Stop();
  }
}

void DefaultGpuHost::Add(mojom::GpuRequest request) {
  AddInternal(std::move(request));
}

void DefaultGpuHost::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
#if defined(OS_WIN)
  gfx::RenderingWindowManager::GetInstance()->RegisterParent(widget);
#endif
}

void DefaultGpuHost::OnAcceleratedWidgetDestroyed(
    gfx::AcceleratedWidget widget) {
#if defined(OS_WIN)
  gfx::RenderingWindowManager::GetInstance()->UnregisterParent(widget);
#endif
}

void DefaultGpuHost::CreateFrameSinkManager(
    viz::mojom::FrameSinkManagerRequest request,
    viz::mojom::FrameSinkManagerClientPtr client) {
  viz_main_->CreateFrameSinkManager(std::move(request), std::move(client));
}

GpuClient* DefaultGpuHost::AddInternal(mojom::GpuRequest request) {
  auto client(std::make_unique<GpuClient>(
      next_client_id_++, &gpu_info_, &gpu_feature_info_,
      gpu_memory_buffer_manager_.get(), gpu_service_.get()));
  GpuClient* client_ref = client.get();
  gpu_bindings_.AddBinding(std::move(client), std::move(request));
  return client_ref;
}

void DefaultGpuHost::OnBadMessageFromGpu() {
  // TODO(sad): Received some unexpected message from the gpu process. We
  // should kill the process and restart it.
  NOTIMPLEMENTED();
}

void DefaultGpuHost::InitializeVizMain(viz::mojom::VizMainRequest request) {
  viz::VizMainImpl::ExternalDependencies deps;
  deps.create_display_compositor = true;
  viz_main_impl_ = std::make_unique<viz::VizMainImpl>(nullptr, std::move(deps));
  viz_main_impl_->Bind(std::move(request));
  viz_main_wait_.Signal();
}

void DefaultGpuHost::DidInitialize(
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info) {
  gpu_info_ = gpu_info;
  gpu_feature_info_ = gpu_feature_info;
  delegate_->OnGpuServiceInitialized();
}

void DefaultGpuHost::DidFailInitialize() {}

void DefaultGpuHost::DidCreateOffscreenContext(const GURL& url) {}

void DefaultGpuHost::DidDestroyOffscreenContext(const GURL& url) {}

void DefaultGpuHost::DidDestroyChannel(int32_t client_id) {}

void DefaultGpuHost::DidLoseContext(bool offscreen,
                                    gpu::error::ContextLostReason reason,
                                    const GURL& active_url) {}

void DefaultGpuHost::SetChildSurface(gpu::SurfaceHandle parent,
                                     gpu::SurfaceHandle child) {
#if defined(OS_WIN)
  // Verify that |parent| was created by the window server.
  DWORD process_id = 0;
  DWORD thread_id = GetWindowThreadProcessId(parent, &process_id);
  if (!thread_id || process_id != ::GetCurrentProcessId()) {
    OnBadMessageFromGpu();
    return;
  }

  // TODO(sad): Also verify that |child| was created by the mus-gpu process.

  if (!gfx::RenderingWindowManager::GetInstance()->RegisterChild(parent,
                                                                 child)) {
    OnBadMessageFromGpu();
  }
#else
  NOTREACHED();
#endif
}

void DefaultGpuHost::StoreShaderToDisk(int32_t client_id,
                                       const std::string& key,
                                       const std::string& shader) {}

void DefaultGpuHost::RecordLogMessage(int32_t severity,
                                      const std::string& header,
                                      const std::string& message) {}

}  // namespace ws
}  // namespace ui
