// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_H_

#include <memory>

#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/video_capture/public/interfaces/video_capture_service.mojom.h"

namespace video_capture {

class VideoCaptureDeviceFactoryImpl;

// Implementation of mojom::VideoCaptureService as a Service Manager service.
class VideoCaptureService
    : public service_manager::Service,
      public service_manager::InterfaceFactory<mojom::VideoCaptureService>,
      public mojom::VideoCaptureService {
 public:
  VideoCaptureService();
  ~VideoCaptureService() override;

  // service_manager::Service:
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

  // service_manager::InterfaceFactory<mojom::VideoCaptureService>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::VideoCaptureServiceRequest request) override;

  // mojom::VideoCaptureService
  void ConnectToDeviceFactory(
      mojom::VideoCaptureDeviceFactoryRequest request) override;
  void ConnectToFakeDeviceFactory(
      mojom::VideoCaptureDeviceFactoryRequest request) override;
  void ConnectToMockDeviceFactory(
      mojom::VideoCaptureDeviceFactoryRequest request) override;
  void AddDeviceToMockFactory(
      mojom::MockVideoCaptureDevicePtr device,
      mojom::VideoCaptureDeviceDescriptorPtr descriptor,
      const AddDeviceToMockFactoryCallback& callback) override;

 private:
  void LazyInitializeDeviceFactory();
  void LazyInitializeFakeDeviceFactory();
  void LazyInitializeMockDeviceFactory();

  mojo::BindingSet<mojom::VideoCaptureService> service_bindings_;
  mojo::BindingSet<mojom::VideoCaptureDeviceFactory> factory_bindings_;
  mojo::BindingSet<mojom::VideoCaptureDeviceFactory> fake_factory_bindings_;
  mojo::BindingSet<mojom::VideoCaptureDeviceFactory> mock_factory_bindings_;
  std::unique_ptr<VideoCaptureDeviceFactoryImpl> device_factory_;
  std::unique_ptr<VideoCaptureDeviceFactoryImpl> fake_device_factory_;
  std::unique_ptr<VideoCaptureDeviceFactoryImpl> mock_device_factory_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_H_
