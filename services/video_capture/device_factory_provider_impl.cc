// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/device_factory_provider_impl.h"

#include "media/capture/video/fake_video_capture_device_factory.h"
#include "media/capture/video/video_capture_buffer_pool.h"
#include "media/capture/video/video_capture_buffer_tracker.h"
#include "media/capture/video/video_capture_system_impl.h"
#include "services/ui/public/cpp/gpu/client_gpu_memory_buffer_manager.h"
#include "services/ui/public/cpp/gpu/gpu.h"
#include "services/video_capture/device_factory_media_to_mojo_adapter.h"
#include "services/video_capture/virtual_device_enabled_device_factory.h"

namespace video_capture {

DeviceFactoryProviderImpl::DeviceFactoryProviderImpl(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref,
    base::Callback<void(float)> set_shutdown_delay_cb)
    : service_ref_(std::move(service_ref)),
      set_shutdown_delay_cb_(std::move(set_shutdown_delay_cb)),
      weak_factory_(this) {}

DeviceFactoryProviderImpl::~DeviceFactoryProviderImpl() {}

void DeviceFactoryProviderImpl::InjectGpuDependencies(
    ui::mojom::GpuMemoryBufferFactoryPtr memory_buffer_factory,
    mojom::AcceleratorFactoryPtr accelerator_factory) {
  accelerator_factory_ = std::move(accelerator_factory);
// Since the instance of ui::ClientGpuMemoryBufferManager seems kind of
// expensive, we only create it on platforms where it gets actually used.
#if defined(OS_CHROMEOS)
  gpu_memory_buffer_manager_ =
      std::make_unique<ui::ClientGpuMemoryBufferManager>(
          std::move(memory_buffer_factory));
#endif
}

void DeviceFactoryProviderImpl::ConnectToDeviceFactory(
    mojom::DeviceFactoryRequest request) {
  LazyInitializeDeviceFactory();
  factory_bindings_.AddBinding(device_factory_.get(), std::move(request));
}

void DeviceFactoryProviderImpl::SetShutdownDelayInSeconds(float seconds) {
  set_shutdown_delay_cb_.Run(seconds);
}

void DeviceFactoryProviderImpl::LazyInitializeDeviceFactory() {
  if (device_factory_)
    return;

  // Create the platform-specific device factory.
  // The task runner passed to CreateFactory is used for things that need to
  // happen on a "UI thread equivalent", e.g. obtaining screen rotation on
  // Chrome OS.
  std::unique_ptr<media::VideoCaptureDeviceFactory> media_device_factory =
      media::VideoCaptureDeviceFactory::CreateFactory(
          base::ThreadTaskRunnerHandle::Get(), gpu_memory_buffer_manager_.get(),
          base::BindRepeating(
              &DeviceFactoryProviderImpl::CreateJpegDecodeAccelerator,
              weak_factory_.GetWeakPtr()),
          base::BindRepeating(
              &DeviceFactoryProviderImpl::CreateJpegEncodeAccelerator,
              weak_factory_.GetWeakPtr()));
  auto video_capture_system = std::make_unique<media::VideoCaptureSystemImpl>(
      std::move(media_device_factory));

  device_factory_ = std::make_unique<VirtualDeviceEnabledDeviceFactory>(
      service_ref_->Clone(),
      std::make_unique<DeviceFactoryMediaToMojoAdapter>(
          service_ref_->Clone(), std::move(video_capture_system),
          base::BindRepeating(
              &DeviceFactoryProviderImpl::CreateJpegDecodeAccelerator,
              weak_factory_.GetWeakPtr())));
}

void DeviceFactoryProviderImpl::CreateJpegDecodeAccelerator(
    media::mojom::JpegDecodeAcceleratorRequest request) {
  if (!accelerator_factory_)
    return;
  accelerator_factory_->CreateJpegDecodeAccelerator(std::move(request));
}

void DeviceFactoryProviderImpl::CreateJpegEncodeAccelerator(
    media::mojom::JpegEncodeAcceleratorRequest request) {
  if (!accelerator_factory_)
    return;
  accelerator_factory_->CreateJpegEncodeAccelerator(std::move(request));
}

}  // namespace video_capture
