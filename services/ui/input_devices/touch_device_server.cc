// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/input_devices/touch_device_server.h"

#include <utility>
#include <vector>

#include "services/service_manager/public/cpp/bind_source_info.h"
#include "ui/display/manager/chromeos/default_touch_transform_setter.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchscreen_device.h"

namespace ui {

TouchDeviceServer::TouchDeviceServer()
    : touch_transform_setter_(
          base::MakeUnique<display::DefaultTouchTransformSetter>()) {}

TouchDeviceServer::~TouchDeviceServer() {}

void TouchDeviceServer::AddInterface(
    service_manager::BinderRegistry* registry) {
  registry->AddInterface<mojom::TouchDeviceServer>(
      base::Bind(&TouchDeviceServer::BindTouchDeviceServerRequest,
                 base::Unretained(this)));
}

void TouchDeviceServer::ConfigureTouchDevices(
    const std::unordered_map<int32_t, double>& transport_scales,
    const std::vector<display::TouchDeviceTransform>& transforms) {
  std::map<int32_t, double> scales(transport_scales.begin(),
                                   transport_scales.end());
  touch_transform_setter_->ConfigureTouchDevices(scales, transforms);
}

void TouchDeviceServer::BindTouchDeviceServerRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::TouchDeviceServerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace ui
