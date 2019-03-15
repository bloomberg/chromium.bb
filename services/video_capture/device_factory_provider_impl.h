// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_PROVIDER_H_
#define SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_PROVIDER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "media/capture/video/video_capture_jpeg_decoder.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "services/video_capture/public/mojom/device_factory.mojom.h"
#include "services/video_capture/public/mojom/device_factory_provider.mojom.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"

#if defined(OS_CHROMEOS)
#include "media/capture/video/chromeos/mojo/cros_image_capture.mojom.h"
#endif  // defined(OS_CHROMEOS)

namespace video_capture {

class VirtualDeviceEnabledDeviceFactory;

class DeviceFactoryProviderImpl : public mojom::DeviceFactoryProvider {
 public:
  explicit DeviceFactoryProviderImpl(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      base::OnceClosure request_service_quit_asap_cb);
  ~DeviceFactoryProviderImpl() override;

  void SetServiceRef(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);

  // mojom::DeviceFactoryProvider implementation.
  void InjectGpuDependencies(
      mojom::AcceleratorFactoryPtr accelerator_factory) override;
  void ConnectToDeviceFactory(mojom::DeviceFactoryRequest request) override;
  void ConnectToVideoSourceProvider(
      mojom::VideoSourceProviderRequest request) override;
  void ShutdownServiceAsap() override;
  void SetRetryCount(int32_t count) override;

#if defined(OS_CHROMEOS)
  void BindCrosImageCaptureRequest(
      cros::mojom::CrosImageCaptureRequest request);
#endif  // defined(OS_CHROMEOS)

 private:
  class GpuDependenciesContext;

  void LazyInitializeGpuDependenciesContext();
  void LazyInitializeDeviceFactory();
  void LazyInitializeVideoSourceProvider();
  void OnFactoryClientDisconnected();

  mojo::BindingSet<mojom::DeviceFactory> factory_bindings_;
  std::unique_ptr<VirtualDeviceEnabledDeviceFactory> device_factory_;
  mojo::BindingSet<mojom::VideoSourceProvider> video_source_provider_bindings_;
  std::unique_ptr<mojom::VideoSourceProvider> video_source_provider_;
  std::unique_ptr<service_manager::ServiceContextRef> service_ref_;
  std::unique_ptr<GpuDependenciesContext> gpu_dependencies_context_;

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  base::OnceClosure request_service_quit_asap_cb_;

  DISALLOW_COPY_AND_ASSIGN(DeviceFactoryProviderImpl);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_PROVIDER_H_
