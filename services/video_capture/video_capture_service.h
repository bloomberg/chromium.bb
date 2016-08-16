// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_H_

#include <memory>

#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/service.h"
#include "services/video_capture/public/interfaces/video_capture_device_factory.mojom.h"
#include "services/video_capture/video_capture_device_factory_impl.h"

namespace video_capture {

// Exposes a single internal instance of VideoCaptureDeviceFactoryImpl
// through a Mojo Shell Service.
class VideoCaptureService
    : public shell::Service,
      public shell::InterfaceFactory<mojom::VideoCaptureDeviceFactory> {
 public:
  VideoCaptureService();
  ~VideoCaptureService() override;

  // shell::Service:
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) override;

  // shell::InterfaceFactory<mojom::VideoCaptureDeviceFactory>:
  void Create(const shell::Identity& remote_identity,
              mojom::VideoCaptureDeviceFactoryRequest request) override;

 private:
  mojo::BindingSet<mojom::VideoCaptureDeviceFactory> bindings_;
  VideoCaptureDeviceFactoryImpl device_factory_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_H_
