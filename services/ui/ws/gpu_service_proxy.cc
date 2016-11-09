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
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/ui/ws/gpu_service_proxy_delegate.h"
#include "services/ui/ws/mus_gpu_memory_buffer_manager.h"
#include "ui/gfx/buffer_format_util.h"

namespace ui {
namespace ws {

namespace {

const int32_t kInternalGpuChannelClientId = 1;
const uint64_t kInternalGpuChannelClientTracingId = 1;

}  // namespace

GpuServiceProxy::GpuServiceProxy(GpuServiceProxyDelegate* delegate)
    : delegate_(delegate),
      next_client_id_(kInternalGpuChannelClientId + 1),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      shutdown_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                      base::WaitableEvent::InitialState::NOT_SIGNALED) {
  gpu_main_.OnStart();
  // TODO(sad): Once GPU process is split, this would look like:
  //   connector->ConnectToInterface("service:gpu", &gpu_service_);
  gpu_main_.Create(GetProxy(&gpu_service_));
  gpu_service_->Initialize(
      base::Bind(&GpuServiceProxy::OnInitialized, base::Unretained(this)));
}

GpuServiceProxy::~GpuServiceProxy() {
  if (gpu_channel_)
    gpu_channel_->DestroyChannel();
}

void GpuServiceProxy::Add(mojom::GpuServiceRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void GpuServiceProxy::OnInitialized(const gpu::GPUInfo& gpu_info) {
  gpu_info_ = gpu_info;

  constexpr bool is_gpu_host = true;
  gpu_service_->EstablishGpuChannel(
      kInternalGpuChannelClientId, kInternalGpuChannelClientTracingId,
      is_gpu_host, base::Bind(&GpuServiceProxy::OnInternalGpuChannelEstablished,
                             base::Unretained(this)));
}

void GpuServiceProxy::OnInternalGpuChannelEstablished(
    mojo::ScopedMessagePipeHandle channel_handle) {
  io_thread_ = base::MakeUnique<base::Thread>("GPUIOThread");
  base::Thread::Options thread_options(base::MessageLoop::TYPE_IO, 0);
  thread_options.priority = base::ThreadPriority::NORMAL;
  CHECK(io_thread_->StartWithOptions(thread_options));

  gpu_memory_buffer_manager_ = base::MakeUnique<MusGpuMemoryBufferManager>(
      gpu_service_.get(), kInternalGpuChannelClientId);
  gpu_channel_ = gpu::GpuChannelHost::Create(
      this, kInternalGpuChannelClientId, gpu_info_,
      IPC::ChannelHandle(channel_handle.release()), &shutdown_event_,
      gpu_memory_buffer_manager_.get());
  if (delegate_)
    delegate_->OnGpuChannelEstablished(gpu_channel_);
}

void GpuServiceProxy::OnGpuChannelEstablished(
    const EstablishGpuChannelCallback& callback,
    int32_t client_id,
    mojo::ScopedMessagePipeHandle channel_handle) {
  callback.Run(client_id, std::move(channel_handle), gpu_info_);
}

void GpuServiceProxy::EstablishGpuChannel(
    const EstablishGpuChannelCallback& callback) {
  const int client_id = next_client_id_++;
  // TODO(sad): crbug.com/617415 figure out how to generate a meaningful tracing
  // id.
  const uint64_t client_tracing_id = 0;
  constexpr bool is_gpu_host = false;
  gpu_service_->EstablishGpuChannel(
      client_id, client_tracing_id, is_gpu_host,
      base::Bind(&GpuServiceProxy::OnGpuChannelEstablished,
                 base::Unretained(this), callback, client_id));
}

void GpuServiceProxy::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    const mojom::GpuService::CreateGpuMemoryBufferCallback& callback) {
  // TODO(sad): Check to see if native gpu memory buffer can be used first.
  if (!gpu::GpuMemoryBufferImplSharedMemory::IsUsageSupported(usage) ||
      !gpu::GpuMemoryBufferImplSharedMemory::IsSizeValidForFormat(size,
                                                                  format)) {
    callback.Run(gfx::GpuMemoryBufferHandle());
    return;
  }

  size_t bytes = 0;
  if (!gfx::BufferSizeForBufferFormatChecked(size, format, &bytes)) {
    callback.Run(gfx::GpuMemoryBufferHandle());
    return;
  }

  mojo::ScopedSharedBufferHandle mojo_handle =
      mojo::SharedBufferHandle::Create(bytes);
  if (!mojo_handle.is_valid()) {
    callback.Run(gfx::GpuMemoryBufferHandle());
    return;
  }

  base::SharedMemoryHandle shm_handle;
  size_t shm_size;
  bool readonly;
  MojoResult result = mojo::UnwrapSharedMemoryHandle(
      std::move(mojo_handle), &shm_handle, &shm_size, &readonly);
  if (result != MOJO_RESULT_OK) {
    callback.Run(gfx::GpuMemoryBufferHandle());
    return;
  }
  DCHECK_EQ(shm_size, bytes);
  DCHECK(!readonly);
  const int stride = base::checked_cast<int>(
      gfx::RowSizeForBufferFormat(size.width(), format, 0));

  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::SHARED_MEMORY_BUFFER;
  gmb_handle.id = id;
  gmb_handle.handle = shm_handle;
  gmb_handle.offset = 0;
  gmb_handle.stride = stride;
  callback.Run(gmb_handle);
}

void GpuServiceProxy::DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                             const gpu::SyncToken& sync_token) {
  //  NOTIMPLEMENTED();
}

bool GpuServiceProxy::IsMainThread() {
  return main_thread_task_runner_->BelongsToCurrentThread();
}

scoped_refptr<base::SingleThreadTaskRunner>
GpuServiceProxy::GetIOThreadTaskRunner() {
  return io_thread_->task_runner();
}

std::unique_ptr<base::SharedMemory> GpuServiceProxy::AllocateSharedMemory(
    size_t size) {
  std::unique_ptr<base::SharedMemory> shm(new base::SharedMemory());
  if (!shm->CreateAnonymous(size))
    shm.reset();
  return shm;
}

}  // namespace ws
}  // namespace ui
