// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/gpu/gpu.h"

#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/cpp/gpu/client_gpu_memory_buffer_manager.h"
#include "services/ui/public/cpp/gpu/context_provider_command_buffer.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/gpu.mojom.h"

namespace ui {

Gpu::Gpu(service_manager::Connector* connector,
         service_manager::InterfaceProvider* provider,
         scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(std::move(task_runner)),
      connector_(connector),
      interface_provider_(provider),
      shutdown_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                      base::WaitableEvent::InitialState::NOT_SIGNALED) {
  DCHECK(main_task_runner_);
  DCHECK(connector_ || interface_provider_);
  mojom::GpuPtr gpu_ptr;
  if (connector_)
    connector_->BindInterface(ui::mojom::kServiceName, &gpu_ptr);
  else
    interface_provider_->GetInterface(&gpu_ptr);
  gpu_memory_buffer_manager_ =
      base::MakeUnique<ClientGpuMemoryBufferManager>(std::move(gpu_ptr));
  if (!io_task_runner_) {
    io_thread_.reset(new base::Thread("GPUIOThread"));
    base::Thread::Options thread_options(base::MessageLoop::TYPE_IO, 0);
    thread_options.priority = base::ThreadPriority::NORMAL;
    CHECK(io_thread_->StartWithOptions(thread_options));
    io_task_runner_ = io_thread_->task_runner();
  }
}

Gpu::~Gpu() {
  DCHECK(IsMainThread());
  shutdown_event_.Signal();
  if (gpu_channel_)
    gpu_channel_->DestroyChannel();
}

// static
std::unique_ptr<Gpu> Gpu::Create(
    service_manager::Connector* connector,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return base::WrapUnique(new Gpu(connector, nullptr, std::move(task_runner)));
}

std::unique_ptr<Gpu> Gpu::Create(
    service_manager::InterfaceProvider* provider,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return base::WrapUnique(new Gpu(nullptr, provider, std::move(task_runner)));
}

scoped_refptr<cc::ContextProvider> Gpu::CreateContextProvider(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel) {
  constexpr bool automatic_flushes = false;
  constexpr bool support_locking = false;
  gpu::gles2::ContextCreationAttribHelper attributes;
  attributes.alpha_size = -1;
  attributes.depth_size = 0;
  attributes.stencil_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  attributes.lose_context_when_out_of_memory = true;
  constexpr ui::ContextProviderCommandBuffer* shared_context_provider = nullptr;
  return make_scoped_refptr(new ui::ContextProviderCommandBuffer(
      std::move(gpu_channel), gpu::GPU_STREAM_DEFAULT,
      gpu::GpuStreamPriority::NORMAL, gpu::kNullSurfaceHandle,
      GURL("chrome://gpu/MusContextFactory"), automatic_flushes,
      support_locking, gpu::SharedMemoryLimits(), attributes,
      shared_context_provider, ui::command_buffer_metrics::MUS_CLIENT_CONTEXT));
}

void Gpu::EstablishGpuChannel(
    const gpu::GpuChannelEstablishedCallback& callback) {
  DCHECK(IsMainThread());
  scoped_refptr<gpu::GpuChannelHost> channel = GetGpuChannel();
  if (channel) {
    main_task_runner_->PostTask(FROM_HERE,
                                base::Bind(callback, std::move(channel)));
    return;
  }
  establish_callbacks_.push_back(callback);
  if (gpu_)
    return;

  if (connector_)
    connector_->BindInterface(ui::mojom::kServiceName, &gpu_);
  else
    interface_provider_->GetInterface(&gpu_);
  gpu_->EstablishGpuChannel(
      base::Bind(&Gpu::OnEstablishedGpuChannel, base::Unretained(this)));
}

scoped_refptr<gpu::GpuChannelHost> Gpu::EstablishGpuChannelSync() {
  DCHECK(IsMainThread());
  if (GetGpuChannel())
    return gpu_channel_;

  int client_id = 0;
  mojo::ScopedMessagePipeHandle channel_handle;
  gpu::GPUInfo gpu_info;
  if (connector_)
    connector_->BindInterface(ui::mojom::kServiceName, &gpu_);
  else
    interface_provider_->GetInterface(&gpu_);

  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  if (!gpu_->EstablishGpuChannel(&client_id, &channel_handle, &gpu_info)) {
    DLOG(WARNING)
        << "Channel encountered error while establishing gpu channel.";
    return nullptr;
  }
  OnEstablishedGpuChannel(client_id, std::move(channel_handle), gpu_info);
  return gpu_channel_;
}

gpu::GpuMemoryBufferManager* Gpu::GetGpuMemoryBufferManager() {
  return gpu_memory_buffer_manager_.get();
}

scoped_refptr<gpu::GpuChannelHost> Gpu::GetGpuChannel() {
  DCHECK(IsMainThread());
  if (gpu_channel_ && gpu_channel_->IsLost()) {
    gpu_channel_->DestroyChannel();
    gpu_channel_ = nullptr;
  }
  return gpu_channel_;
}

void Gpu::OnEstablishedGpuChannel(int client_id,
                                  mojo::ScopedMessagePipeHandle channel_handle,
                                  const gpu::GPUInfo& gpu_info) {
  DCHECK(IsMainThread());
  DCHECK(gpu_.get());
  DCHECK(!gpu_channel_);

  if (client_id && channel_handle.is_valid()) {
    gpu_channel_ = gpu::GpuChannelHost::Create(
        this, client_id, gpu_info, IPC::ChannelHandle(channel_handle.release()),
        &shutdown_event_, gpu_memory_buffer_manager_.get());
  }

  gpu_.reset();
  for (const auto& i : establish_callbacks_)
    i.Run(gpu_channel_);
  establish_callbacks_.clear();
}

bool Gpu::IsMainThread() {
  return main_task_runner_->BelongsToCurrentThread();
}

scoped_refptr<base::SingleThreadTaskRunner> Gpu::GetIOThreadTaskRunner() {
  return io_task_runner_;
}

std::unique_ptr<base::SharedMemory> Gpu::AllocateSharedMemory(size_t size) {
  mojo::ScopedSharedBufferHandle handle =
      mojo::SharedBufferHandle::Create(size);
  if (!handle.is_valid())
    return nullptr;

  base::SharedMemoryHandle platform_handle;
  size_t shared_memory_size;
  bool readonly;
  MojoResult result = mojo::UnwrapSharedMemoryHandle(
      std::move(handle), &platform_handle, &shared_memory_size, &readonly);
  if (result != MOJO_RESULT_OK)
    return nullptr;
  DCHECK_EQ(shared_memory_size, size);

  return base::MakeUnique<base::SharedMemory>(platform_handle, readonly);
}

}  // namespace ui
