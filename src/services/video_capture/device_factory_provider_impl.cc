// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/device_factory_provider_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "media/capture/video/create_video_capture_device_factory.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "media/capture/video/video_capture_buffer_pool.h"
#include "media/capture/video/video_capture_buffer_tracker.h"
#include "media/capture/video/video_capture_system_impl.h"
#include "services/video_capture/device_factory_media_to_mojo_adapter.h"
#include "services/video_capture/video_source_provider_impl.h"
#include "services/video_capture/virtual_device_enabled_device_factory.h"
#include "services/ws/public/cpp/gpu/gpu.h"

namespace video_capture {

// Intended usage of this class is to instantiate on any sequence, and then
// operate and release the instance on the task runner exposed via
// GetTaskRunner() via WeakPtrs provided via GetWeakPtr(). To this end,
// GetTaskRunner() and GetWeakPtr() can be called from any sequence, typically
// the same as the one calling the constructor.
class DeviceFactoryProviderImpl::GpuDependenciesContext {
 public:
  GpuDependenciesContext() : weak_factory_for_gpu_io_thread_(this) {
    gpu_io_task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
        {base::TaskPriority::BEST_EFFORT, base::MayBlock()});
  }

  ~GpuDependenciesContext() {
    DCHECK(gpu_io_task_runner_->RunsTasksInCurrentSequence());
  }

  base::WeakPtr<GpuDependenciesContext> GetWeakPtr() {
    return weak_factory_for_gpu_io_thread_.GetWeakPtr();
  }

  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() {
    return gpu_io_task_runner_;
  }

  void InjectGpuDependencies(
      mojom::AcceleratorFactoryPtrInfo accelerator_factory_info) {
    DCHECK(gpu_io_task_runner_->RunsTasksInCurrentSequence());
    accelerator_factory_.Bind(std::move(accelerator_factory_info));
  }

  void CreateJpegDecodeAccelerator(
      media::mojom::JpegDecodeAcceleratorRequest request) {
    DCHECK(gpu_io_task_runner_->RunsTasksInCurrentSequence());
    if (!accelerator_factory_)
      return;
    accelerator_factory_->CreateJpegDecodeAccelerator(std::move(request));
  }

 private:
  // Task runner for operating |accelerator_factory_| and
  // |gpu_memory_buffer_manager_| on. This must be a different thread from the
  // main service thread in order to avoid a deadlock during shutdown where
  // the main service thread joins a video capture device thread that, in turn,
  // will try to post the release of the jpeg decoder to the thread it is
  // operated on.
  scoped_refptr<base::SequencedTaskRunner> gpu_io_task_runner_;
  mojom::AcceleratorFactoryPtr accelerator_factory_;
  base::WeakPtrFactory<GpuDependenciesContext> weak_factory_for_gpu_io_thread_;
};

DeviceFactoryProviderImpl::DeviceFactoryProviderImpl(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::OnceClosure request_service_quit_asap_cb)
    : ui_task_runner_(std::move(ui_task_runner)),
      request_service_quit_asap_cb_(std::move(request_service_quit_asap_cb)) {
  // Unretained |this| is safe because |factory_bindings_| is owned by
  // |this|.
  factory_bindings_.set_connection_error_handler(base::BindRepeating(
      &DeviceFactoryProviderImpl::OnFactoryOrSourceProviderClientDisconnected,
      base::Unretained(this)));
}

DeviceFactoryProviderImpl::~DeviceFactoryProviderImpl() {
  factory_bindings_.CloseAllBindings();
  device_factory_.reset();
  if (gpu_dependencies_context_) {
    gpu_dependencies_context_->GetTaskRunner()->DeleteSoon(
        FROM_HERE, std::move(gpu_dependencies_context_));
  }
}

void DeviceFactoryProviderImpl::SetServiceRef(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref) {
  service_ref_ = std::move(service_ref);
}

void DeviceFactoryProviderImpl::InjectGpuDependencies(
    mojom::AcceleratorFactoryPtr accelerator_factory) {
  LazyInitializeGpuDependenciesContext();
  gpu_dependencies_context_->GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&GpuDependenciesContext::InjectGpuDependencies,
                                gpu_dependencies_context_->GetWeakPtr(),
                                accelerator_factory.PassInterface()));
}

void DeviceFactoryProviderImpl::ConnectToDeviceFactory(
    mojom::DeviceFactoryRequest request) {
  DCHECK(service_ref_);
  LazyInitializeDeviceFactory();
  factory_bindings_.AddBinding(device_factory_.get(), std::move(request));
}

void DeviceFactoryProviderImpl::ConnectToVideoSourceProvider(
    mojom::VideoSourceProviderRequest request) {
  LazyInitializeVideoSourceProvider();
  video_source_provider_->AddClient(std::move(request));
}

void DeviceFactoryProviderImpl::ShutdownServiceAsap() {
  if (request_service_quit_asap_cb_)
    std::move(request_service_quit_asap_cb_).Run();
}

void DeviceFactoryProviderImpl::LazyInitializeGpuDependenciesContext() {
  if (!gpu_dependencies_context_)
    gpu_dependencies_context_ = std::make_unique<GpuDependenciesContext>();
}

void DeviceFactoryProviderImpl::LazyInitializeDeviceFactory() {
  DCHECK(service_ref_);

  // Factory may already exist but if no client was connected it will not have a
  // ServiceRef.
  if (device_factory_) {
    if (factory_bindings_.empty())
      device_factory_->SetServiceRef(service_ref_->Clone());
    return;
  }

  LazyInitializeGpuDependenciesContext();

  // Create the platform-specific device factory.
  // The task runner passed to CreateFactory is used for things that need to
  // happen on a "UI thread equivalent", e.g. obtaining screen rotation on
  // Chrome OS.
  std::unique_ptr<media::VideoCaptureDeviceFactory> media_device_factory =
      media::CreateVideoCaptureDeviceFactory(ui_task_runner_);
  DCHECK(media_device_factory);

  auto video_capture_system = std::make_unique<media::VideoCaptureSystemImpl>(
      std::move(media_device_factory));

  device_factory_ = std::make_unique<VirtualDeviceEnabledDeviceFactory>(
      std::make_unique<DeviceFactoryMediaToMojoAdapter>(
          std::move(video_capture_system),
          base::BindRepeating(
              &GpuDependenciesContext::CreateJpegDecodeAccelerator,
              gpu_dependencies_context_->GetWeakPtr()),
          gpu_dependencies_context_->GetTaskRunner()));
  device_factory_->SetServiceRef(service_ref_->Clone());
}

void DeviceFactoryProviderImpl::LazyInitializeVideoSourceProvider() {
  if (video_source_provider_)
    return;
  LazyInitializeDeviceFactory();
  video_source_provider_ = std::make_unique<VideoSourceProviderImpl>(
      device_factory_.get(),
      // Unretained(this) is safe, because |this| owns |video_source_provider_|.
      base::BindRepeating(
          &DeviceFactoryProviderImpl::OnLastSourceProviderClientDisconnected,
          base::Unretained(this)));
}

void DeviceFactoryProviderImpl::OnLastSourceProviderClientDisconnected() {
  video_source_provider_.reset();
  OnFactoryOrSourceProviderClientDisconnected();
}

void DeviceFactoryProviderImpl::OnFactoryOrSourceProviderClientDisconnected() {
  // If |video_source_provider_| still exists, it means there is still a client
  // connected to it, in which case we also still need |device_factory_| to
  // stay operational.
  if (video_source_provider_)
    return;

  // If neither |device_factory_| nor |video_source_provider_| have clients
  // connected, release service ref so that service shutdown timeout can start
  // if no other references are still alive. We keep the |device_factory_|
  // instance alive in order to avoid losing state that would be expensive to
  // reinitialize, e.g. having already enumerated the available devices.
  if (factory_bindings_.empty()) {
    device_factory_->SetServiceRef(nullptr);
  }
}

#if defined(OS_CHROMEOS)
void DeviceFactoryProviderImpl::BindCrosImageCaptureRequest(
    cros::mojom::CrosImageCaptureRequest request) {
  CHECK(device_factory_);

  device_factory_->BindCrosImageCaptureRequest(std::move(request));
}
#endif  // defined(OS_CHROMEOS)

}  // namespace video_capture
