// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_PROVIDER_H_
#define SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_PROVIDER_H_

#include <memory>

#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "media/capture/video/video_capture_jpeg_decoder.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "services/video_capture/public/mojom/device_factory_provider.mojom.h"

namespace video_capture {

class DeviceFactoryProviderImpl : public mojom::DeviceFactoryProvider {
 public:
  DeviceFactoryProviderImpl(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref,
      base::Callback<void(float)> set_shutdown_delay_cb);
  ~DeviceFactoryProviderImpl() override;

  // mojom::DeviceFactoryProvider implementation.
  void InjectGpuDependencies(
      ui::mojom::GpuMemoryBufferFactoryPtr memory_buffer_factory,
      mojom::AcceleratorFactoryPtr accelerator_factory) override;
  void ConnectToDeviceFactory(mojom::DeviceFactoryRequest request) override;
  void SetShutdownDelayInSeconds(float seconds) override;

 private:
  void LazyInitializeDeviceFactory();
  void CreateJpegDecodeAccelerator(
      media::mojom::JpegDecodeAcceleratorRequest request);
  void CreateJpegEncodeAccelerator(
      media::mojom::JpegEncodeAcceleratorRequest request);

  mojo::BindingSet<mojom::DeviceFactory> factory_bindings_;
  std::unique_ptr<mojom::DeviceFactory> device_factory_;

  const std::unique_ptr<service_manager::ServiceContextRef> service_ref_;
  mojom::AcceleratorFactoryPtr accelerator_factory_;
  std::unique_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager_;
  base::Callback<void(float)> set_shutdown_delay_cb_;
  base::WeakPtrFactory<DeviceFactoryProviderImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceFactoryProviderImpl);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_PROVIDER_H_
