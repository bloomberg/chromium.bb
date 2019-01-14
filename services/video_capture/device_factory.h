// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_H_
#define SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_H_

#include "services/service_manager/public/cpp/service_context_ref.h"
#include "services/video_capture/public/mojom/device_factory.mojom.h"

namespace video_capture {

class DeviceFactory : public mojom::DeviceFactory {
 public:
  virtual void SetServiceRef(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref) = 0;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_H_
