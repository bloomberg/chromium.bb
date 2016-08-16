// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/video_capture_service.h"

namespace video_capture {

VideoCaptureService::VideoCaptureService() = default;

VideoCaptureService::~VideoCaptureService() = default;

bool VideoCaptureService::OnConnect(const shell::Identity& remote_identity,
                                    shell::InterfaceRegistry* registry) {
  registry->AddInterface<mojom::VideoCaptureDeviceFactory>(this);
  return true;
}

void VideoCaptureService::Create(
    const shell::Identity& remote_identity,
    mojom::VideoCaptureDeviceFactoryRequest request) {
  bindings_.AddBinding(&device_factory_, std::move(request));
}

}  // namespace video_capture
