// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/gpu/gpu_service_internal.h"

#include "base/memory/shared_memory.h"
#include "base/memory/singleton.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/common/memory_stats.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_message_filter.h"
#include "media/gpu/ipc/service/gpu_jpeg_decode_accelerator.h"
#include "media/gpu/ipc/service/gpu_video_decode_accelerator.h"
#include "media/gpu/ipc/service/gpu_video_encode_accelerator.h"
#include "media/gpu/ipc/service/media_service.h"
#include "services/ui/gpu/mus_gpu_memory_buffer_manager.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/init/gl_factory.h"
#include "url/gurl.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace ui {
namespace {

const int kLocalGpuChannelClientId = 1;
const uint64_t kLocalGpuChannelClientTracingId = 1;

void EstablishGpuChannelDone(
    mojo::ScopedMessagePipeHandle* channel_handle,
    const GpuServiceInternal::EstablishGpuChannelCallback& callback) {
  callback.Run(std::move(*channel_handle));
}

}  // namespace

GpuServiceInternal::GpuServiceInternal()
    : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      shutdown_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
      gpu_thread_("GpuThread"),
      io_thread_("GpuIOThread"),
      binding_(this) {}

GpuServiceInternal::~GpuServiceInternal() {
  // Signal this event before destroying the child process.  That way all
  // background threads can cleanup.
  // For example, in the renderer the RenderThread instances will be able to
  // notice shutdown before the render process begins waiting for them to exit.
  shutdown_event_.Signal();
  io_thread_.Stop();
}

void GpuServiceInternal::Add(mojom::GpuServiceInternalRequest request) {
  binding_.Bind(std::move(request));
}

void GpuServiceInternal::EstablishGpuChannelInternal(
    int32_t client_id,
    uint64_t client_tracing_id,
    bool preempts,
    bool allow_view_command_buffers,
    bool allow_real_time_streams,
    const EstablishGpuChannelCallback& callback) {
  DCHECK(CalledOnValidThread());

  if (!gpu_channel_manager_) {
    callback.Run(mojo::ScopedMessagePipeHandle());
    return;
  }

  auto* channel_handle = new mojo::ScopedMessagePipeHandle;
  gpu_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GpuServiceInternal::EstablishGpuChannelOnGpuThread,
                 base::Unretained(this), client_id, client_tracing_id, preempts,
                 allow_view_command_buffers, allow_real_time_streams,
                 base::Unretained(channel_handle)),
      base::Bind(&EstablishGpuChannelDone, base::Owned(channel_handle),
                 callback));
}

gfx::GpuMemoryBufferHandle GpuServiceInternal::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    gpu::SurfaceHandle surface_handle) {
  DCHECK(CalledOnValidThread());
  return gpu_memory_buffer_factory_->CreateGpuMemoryBuffer(
      id, size, format, usage, client_id, surface_handle);
}

void GpuServiceInternal::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id,
    const gpu::SyncToken& sync_token) {
  DCHECK(CalledOnValidThread());

  if (gpu_channel_manager_)
    gpu_channel_manager_->DestroyGpuMemoryBuffer(id, client_id, sync_token);
}

void GpuServiceInternal::DidCreateOffscreenContext(const GURL& active_url) {
  NOTIMPLEMENTED();
}

void GpuServiceInternal::DidDestroyChannel(int client_id) {
  media_service_->RemoveChannel(client_id);
  NOTIMPLEMENTED();
}

void GpuServiceInternal::DidDestroyOffscreenContext(const GURL& active_url) {
  NOTIMPLEMENTED();
}

void GpuServiceInternal::DidLoseContext(bool offscreen,
                                        gpu::error::ContextLostReason reason,
                                        const GURL& active_url) {
  NOTIMPLEMENTED();
}

void GpuServiceInternal::GpuMemoryUmaStats(
    const gpu::GPUMemoryUmaStats& params) {
  NOTIMPLEMENTED();
}

void GpuServiceInternal::StoreShaderToDisk(int client_id,
                                           const std::string& key,
                                           const std::string& shader) {
  NOTIMPLEMENTED();
}

#if defined(OS_WIN)
void GpuServiceInternal::SendAcceleratedSurfaceCreatedChildWindow(
    gpu::SurfaceHandle parent_window,
    gpu::SurfaceHandle child_window) {
  ::SetParent(child_window, parent_window);
}
#endif

void GpuServiceInternal::SetActiveURL(const GURL& url) {
  // TODO(penghuang): implement this function.
}

void GpuServiceInternal::InitializeOnGpuThread(
    mojo::ScopedMessagePipeHandle* channel_handle,
    base::WaitableEvent* event) {
  gpu_info_.video_decode_accelerator_capabilities =
      media::GpuVideoDecodeAccelerator::GetCapabilities(gpu_preferences_);
  gpu_info_.video_encode_accelerator_supported_profiles =
      media::GpuVideoEncodeAccelerator::GetSupportedProfiles(gpu_preferences_);
  gpu_info_.jpeg_decode_accelerator_supported =
      media::GpuJpegDecodeAccelerator::IsSupported();

#if defined(USE_OZONE)
  // TODO(rjkroege): Must plumb the shell::Connector* to here and pass into
  // ozone.
  ui::OzonePlatform::InitializeForGPU();
#endif

  if (gpu::GetNativeGpuMemoryBufferType() != gfx::EMPTY_BUFFER) {
    gpu_memory_buffer_factory_ =
        gpu::GpuMemoryBufferFactory::CreateNativeType();
  }

  if (!gl::init::InitializeGLOneOff())
    VLOG(1) << "gl::init::InitializeGLOneOff failed";

  DCHECK(!owned_sync_point_manager_);
  const bool allow_threaded_wait = false;
  owned_sync_point_manager_.reset(
      new gpu::SyncPointManager(allow_threaded_wait));

  // Defer creation of the render thread. This is to prevent it from handling
  // IPC messages before the sandbox has been enabled and all other necessary
  // initialization has succeeded.
  // TODO(penghuang): implement a watchdog.
  gpu::GpuWatchdog* watchdog = nullptr;
  gpu_channel_manager_.reset(new gpu::GpuChannelManager(
      gpu_preferences_, this, watchdog,
      base::ThreadTaskRunnerHandle::Get().get(), io_thread_.task_runner().get(),
      &shutdown_event_, owned_sync_point_manager_.get(),
      gpu_memory_buffer_factory_.get()));

  media_service_.reset(new media::MediaService(gpu_channel_manager_.get()));

  const bool preempts = true;
  const bool allow_view_command_buffers = true;
  const bool allow_real_time_streams = true;
  EstablishGpuChannelOnGpuThread(
      kLocalGpuChannelClientId, kLocalGpuChannelClientTracingId, preempts,
      allow_view_command_buffers, allow_real_time_streams, channel_handle);
  event->Signal();
}

void GpuServiceInternal::EstablishGpuChannelOnGpuThread(
    int client_id,
    uint64_t client_tracing_id,
    bool preempts,
    bool allow_view_command_buffers,
    bool allow_real_time_streams,
    mojo::ScopedMessagePipeHandle* channel_handle) {
  if (gpu_channel_manager_) {
    auto handle = gpu_channel_manager_->EstablishChannel(
        client_id, client_tracing_id, preempts, allow_view_command_buffers,
        allow_real_time_streams);
    channel_handle->reset(handle.mojo_handle);
    media_service_->AddChannel(client_id);
  }
}

bool GpuServiceInternal::IsMainThread() {
  return main_task_runner_->BelongsToCurrentThread();
}

scoped_refptr<base::SingleThreadTaskRunner>
GpuServiceInternal::GetIOThreadTaskRunner() {
  return io_thread_.task_runner();
}

std::unique_ptr<base::SharedMemory> GpuServiceInternal::AllocateSharedMemory(
    size_t size) {
  std::unique_ptr<base::SharedMemory> shm(new base::SharedMemory());
  if (!shm->CreateAnonymous(size))
    return std::unique_ptr<base::SharedMemory>();
  return shm;
}

void GpuServiceInternal::Initialize(const InitializeCallback& callback) {
  DCHECK(CalledOnValidThread());
  base::Thread::Options thread_options(base::MessageLoop::TYPE_DEFAULT, 0);
  thread_options.priority = base::ThreadPriority::NORMAL;
  CHECK(gpu_thread_.StartWithOptions(thread_options));

  thread_options = base::Thread::Options(base::MessageLoop::TYPE_IO, 0);
  thread_options.priority = base::ThreadPriority::NORMAL;
#if defined(OS_ANDROID)
  // TODO(reveman): Remove this in favor of setting it explicitly for each type
  // of process.
  thread_options.priority = base::ThreadPriority::DISPLAY;
#endif
  CHECK(io_thread_.StartWithOptions(thread_options));

  mojo::ScopedMessagePipeHandle channel_handle;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  gpu_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&GpuServiceInternal::InitializeOnGpuThread,
                            base::Unretained(this), &channel_handle, &event));
  event.Wait();

  gpu_memory_buffer_manager_local_.reset(
      new MusGpuMemoryBufferManager(this, kLocalGpuChannelClientId));
  gpu_channel_local_ = gpu::GpuChannelHost::Create(
      this, kLocalGpuChannelClientId, gpu_info_,
      IPC::ChannelHandle(channel_handle.release()), &shutdown_event_,
      gpu_memory_buffer_manager_local_.get());

  // TODO(sad): Get the real GPUInfo.
  callback.Run(gpu_info_);
}

void GpuServiceInternal::EstablishGpuChannel(
    int32_t client_id,
    uint64_t client_tracing_id,
    const EstablishGpuChannelCallback& callback) {
  // TODO(penghuang): windows server may want to control those flags.
  // Add a private interface for windows server.
  const bool preempts = false;
  const bool allow_view_command_buffers = false;
  const bool allow_real_time_streams = false;
  EstablishGpuChannelInternal(client_id, client_tracing_id, preempts,
                              allow_view_command_buffers,
                              allow_real_time_streams, callback);
}

// static
GpuServiceInternal* GpuServiceInternal::GetInstance() {
  return base::Singleton<GpuServiceInternal,
                         base::LeakySingletonTraits<GpuServiceInternal>>::get();
}

}  // namespace ui
