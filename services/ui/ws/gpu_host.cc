// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/gpu_host.h"

#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl_shared_memory.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/ui/common/server_gpu_memory_buffer_manager.h"
#include "services/ui/ws/gpu_client.h"
#include "services/ui/ws/gpu_host_delegate.h"
#include "ui/gfx/buffer_format_util.h"

#if defined(OS_WIN)
#include "ui/gfx/win/rendering_window_manager.h"
#endif

namespace ui {
namespace ws {

namespace {

// The client Id 1 is reserved for the frame sink manager.
const int32_t kInternalGpuChannelClientId = 2;

}  // namespace

GpuHost::GpuHost(GpuHostDelegate* delegate)
    : delegate_(delegate),
      next_client_id_(kInternalGpuChannelClientId + 1),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      gpu_host_binding_(this) {
  // TODO(sad): Once GPU process is split, this would look like:
  //   connector->BindInterface("gpu", &gpu_main_);
  gpu_main_impl_ = base::MakeUnique<GpuMain>(MakeRequest(&gpu_main_));

  // TODO(sad): Correctly initialize gpu::GpuPreferences (like it is initialized
  // in GpuProcessHost::Init()).
  gpu::GpuPreferences preferences;
  gpu_main_->CreateGpuService(MakeRequest(&gpu_service_),
                              gpu_host_binding_.CreateInterfacePtrAndBind(),
                              preferences, mojo::ScopedSharedBufferHandle());
  gpu_memory_buffer_manager_ = base::MakeUnique<ServerGpuMemoryBufferManager>(
      gpu_service_.get(), next_client_id_++);
}

GpuHost::~GpuHost() {}

void GpuHost::Add(mojom::GpuRequest request) {
  AddInternal(std::move(request));
}

void GpuHost::OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) {
#if defined(OS_WIN)
  gfx::RenderingWindowManager::GetInstance()->RegisterParent(widget);
#endif
}

void GpuHost::OnAcceleratedWidgetDestroyed(gfx::AcceleratedWidget widget) {
#if defined(OS_WIN)
  gfx::RenderingWindowManager::GetInstance()->UnregisterParent(widget);
#endif
}

void GpuHost::CreateFrameSinkManager(
    cc::mojom::FrameSinkManagerRequest request,
    cc::mojom::FrameSinkManagerClientPtr client) {
  gpu_main_->CreateFrameSinkManager(std::move(request), std::move(client));
}

GpuClient* GpuHost::AddInternal(mojom::GpuRequest request) {
  auto client(base::MakeUnique<GpuClient>(next_client_id_++, &gpu_info_,
                                          gpu_memory_buffer_manager_.get(),
                                          gpu_service_.get()));
  GpuClient* client_ref = client.get();
  gpu_bindings_.AddBinding(std::move(client), std::move(request));
  return client_ref;
}

void GpuHost::OnBadMessageFromGpu() {
  // TODO(sad): Received some unexpected message from the gpu process. We
  // should kill the process and restart it.
  NOTIMPLEMENTED();
}

void GpuHost::DidInitialize(const gpu::GPUInfo& gpu_info,
                            const gpu::GpuFeatureInfo& gpu_feature_info) {
  gpu_info_ = gpu_info;
  delegate_->OnGpuServiceInitialized();
}

void GpuHost::DidFailInitialize() {}

void GpuHost::DidCreateOffscreenContext(const GURL& url) {}

void GpuHost::DidDestroyOffscreenContext(const GURL& url) {}

void GpuHost::DidDestroyChannel(int32_t client_id) {}

void GpuHost::DidLoseContext(bool offscreen,
                             gpu::error::ContextLostReason reason,
                             const GURL& active_url) {}

void GpuHost::SetChildSurface(gpu::SurfaceHandle parent,
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

void GpuHost::StoreShaderToDisk(int32_t client_id,
                                const std::string& key,
                                const std::string& shader) {}

void GpuHost::RecordLogMessage(int32_t severity,
                               const std::string& header,
                               const std::string& message) {}

}  // namespace ws
}  // namespace ui
