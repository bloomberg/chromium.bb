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
#include "services/service_manager/public/cpp/connection.h"
#include "services/ui/common/server_gpu_memory_buffer_manager.h"
#include "services/ui/ws/gpu_host_delegate.h"
#include "ui/gfx/buffer_format_util.h"

#if defined(OS_WIN)
#include "ui/gfx/win/rendering_window_manager.h"
#endif

namespace ui {
namespace ws {

namespace {

// The client Id 1 is reserved for the display compositor.
const int32_t kInternalGpuChannelClientId = 2;

// The implementation that relays requests from clients to the real
// service implementation in the GPU process over mojom.GpuService.
class GpuClient : public mojom::Gpu {
 public:
  GpuClient(int client_id,
            gpu::GPUInfo* gpu_info,
            ServerGpuMemoryBufferManager* gpu_memory_buffer_manager,
            mojom::GpuService* gpu_service)
      : client_id_(client_id),
        gpu_info_(gpu_info),
        gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
        gpu_service_(gpu_service) {
    DCHECK(gpu_memory_buffer_manager_);
    DCHECK(gpu_service_);
  }
  ~GpuClient() override {
    gpu_memory_buffer_manager_->DestroyAllGpuMemoryBufferForClient(client_id_);
  }

 private:
  void OnGpuChannelEstablished(const EstablishGpuChannelCallback& callback,
                               mojo::ScopedMessagePipeHandle channel_handle) {
    callback.Run(client_id_, std::move(channel_handle), *gpu_info_);
  }

  // mojom::Gpu overrides:
  void EstablishGpuChannel(
      const EstablishGpuChannelCallback& callback) override {
    // TODO(sad): crbug.com/617415 figure out how to generate a meaningful
    // tracing id.
    const uint64_t client_tracing_id = 0;
    constexpr bool is_gpu_host = false;
    gpu_service_->EstablishGpuChannel(
        client_id_, client_tracing_id, is_gpu_host,
        base::Bind(&GpuClient::OnGpuChannelEstablished, base::Unretained(this),
                   callback));
  }

  void CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      const mojom::Gpu::CreateGpuMemoryBufferCallback& callback) override {
    auto handle = gpu_memory_buffer_manager_->CreateGpuMemoryBufferHandle(
        id, client_id_, size, format, usage, gpu::kNullSurfaceHandle);
    callback.Run(handle);
  }

  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              const gpu::SyncToken& sync_token) override {
    gpu_memory_buffer_manager_->DestroyGpuMemoryBuffer(id, client_id_,
                                                       sync_token);
  }

  const int client_id_;

  // The objects these pointers refer to are owned by the GpuHost object.
  const gpu::GPUInfo* gpu_info_;
  ServerGpuMemoryBufferManager* gpu_memory_buffer_manager_;
  mojom::GpuService* gpu_service_;

  DISALLOW_COPY_AND_ASSIGN(GpuClient);
};

}  // namespace

GpuHost::GpuHost(GpuHostDelegate* delegate)
    : delegate_(delegate),
      next_client_id_(kInternalGpuChannelClientId + 1),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      gpu_host_binding_(this) {
  // TODO(sad): Once GPU process is split, this would look like:
  //   connector->BindInterface("gpu", &gpu_main_);
  gpu_main_impl_ = base::MakeUnique<GpuMain>(MakeRequest(&gpu_main_));
  gpu_main_impl_->OnStart();

  // TODO(sad): Correctly initialize gpu::GpuPreferences (like it is initialized
  // in GpuProcessHost::Init()).
  gpu::GpuPreferences preferences;
  gpu_main_->CreateGpuService(MakeRequest(&gpu_service_),
                              gpu_host_binding_.CreateInterfacePtrAndBind(),
                              preferences);
  gpu_memory_buffer_manager_ = base::MakeUnique<ServerGpuMemoryBufferManager>(
      gpu_service_.get(), next_client_id_++);
}

GpuHost::~GpuHost() {}

void GpuHost::Add(mojom::GpuRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<GpuClient>(next_client_id_++, &gpu_info_,
                                  gpu_memory_buffer_manager_.get(),
                                  gpu_service_.get()),
      std::move(request));
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

void GpuHost::CreateDisplayCompositor(
    cc::mojom::DisplayCompositorRequest request,
    cc::mojom::DisplayCompositorClientPtr client) {
  gpu_main_->CreateDisplayCompositor(std::move(request), std::move(client));
}

void GpuHost::OnBadMessageFromGpu() {
  // TODO(sad): Received some unexpected message from the gpu process. We
  // should kill the process and restart it.
  NOTIMPLEMENTED();
}

void GpuHost::DidInitialize(const gpu::GPUInfo& gpu_info) {
  gpu_info_ = gpu_info;
  delegate_->OnGpuServiceInitialized();
}

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

}  // namespace ws
}  // namespace ui
