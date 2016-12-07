// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/gpu_service_proxy.h"

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
#include "services/ui/common/mus_gpu_memory_buffer_manager.h"
#include "services/ui/ws/gpu_service_proxy_delegate.h"
#include "ui/gfx/buffer_format_util.h"

namespace ui {
namespace ws {

namespace {

// The client Id 1 is reserved for the display compositor.
const int32_t kInternalGpuChannelClientId = 2;

// The implementation that relays requests from clients to the real
// service implementation in the GPU process over mojom.GpuServiceInternal.
class GpuServiceImpl : public mojom::GpuService {
 public:
  GpuServiceImpl(int client_id,
                 gpu::GPUInfo* gpu_info,
                 MusGpuMemoryBufferManager* gpu_memory_buffer_manager,
                 mojom::GpuServiceInternal* gpu_service_internal)
      : client_id_(client_id),
        gpu_info_(gpu_info),
        gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
        gpu_service_internal_(gpu_service_internal) {
    DCHECK(gpu_memory_buffer_manager_);
    DCHECK(gpu_service_internal_);
  }
  ~GpuServiceImpl() override {
    gpu_memory_buffer_manager_->DestroyAllGpuMemoryBufferForClient(client_id_);
  }

 private:
  void OnGpuChannelEstablished(const EstablishGpuChannelCallback& callback,
                               mojo::ScopedMessagePipeHandle channel_handle) {
    callback.Run(client_id_, std::move(channel_handle), *gpu_info_);
  }

  // mojom::GpuService overrides:
  void EstablishGpuChannel(
      const EstablishGpuChannelCallback& callback) override {
    // TODO(sad): crbug.com/617415 figure out how to generate a meaningful
    // tracing id.
    const uint64_t client_tracing_id = 0;
    constexpr bool is_gpu_host = false;
    gpu_service_internal_->EstablishGpuChannel(
        client_id_, client_tracing_id, is_gpu_host,
        base::Bind(&GpuServiceImpl::OnGpuChannelEstablished,
                   base::Unretained(this), callback));
  }

  void CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      const mojom::GpuService::CreateGpuMemoryBufferCallback& callback)
      override {
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

  // The objects these pointers refer to are owned by the GpuServiceProxy
  // object.
  const gpu::GPUInfo* gpu_info_;
  MusGpuMemoryBufferManager* gpu_memory_buffer_manager_;
  mojom::GpuServiceInternal* gpu_service_internal_;

  DISALLOW_COPY_AND_ASSIGN(GpuServiceImpl);
};

}  // namespace

GpuServiceProxy::GpuServiceProxy(GpuServiceProxyDelegate* delegate)
    : delegate_(delegate),
      next_client_id_(kInternalGpuChannelClientId + 1),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  gpu_main_impl_ = base::MakeUnique<GpuMain>(GetProxy(&gpu_main_));
  gpu_main_impl_->OnStart();
  // TODO(sad): Once GPU process is split, this would look like:
  //   connector->ConnectToInterface("gpu", &gpu_main_);
  gpu_main_->CreateGpuService(
      GetProxy(&gpu_service_),
      base::Bind(&GpuServiceProxy::OnInitialized, base::Unretained(this)));
  gpu_memory_buffer_manager_ = base::MakeUnique<MusGpuMemoryBufferManager>(
      gpu_service_.get(), next_client_id_++);
}

GpuServiceProxy::~GpuServiceProxy() {
}

void GpuServiceProxy::Add(mojom::GpuServiceRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<GpuServiceImpl>(next_client_id_++, &gpu_info_,
                                       gpu_memory_buffer_manager_.get(),
                                       gpu_service_.get()),
      std::move(request));
}

void GpuServiceProxy::CreateDisplayCompositor(
    cc::mojom::DisplayCompositorRequest request,
    cc::mojom::DisplayCompositorClientPtr client) {
  gpu_main_->CreateDisplayCompositor(std::move(request), std::move(client));
}

void GpuServiceProxy::OnInitialized(const gpu::GPUInfo& gpu_info) {
  gpu_info_ = gpu_info;
  delegate_->OnGpuServiceInitialized();
}

}  // namespace ws
}  // namespace ui
