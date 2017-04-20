// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/device_light/DeviceLightDispatcher.h"

#include <cmath>

#include "modules/device_light/DeviceLightController.h"
#include "public/platform/Platform.h"

namespace {
double EnsureRoundedLuxValue(double lux) {
  // Make sure to round the lux value to nearest integer, to
  // avoid too precise values and hence reduce fingerprinting risk.
  // The special case when the lux value is infinity (no data can be
  // provided) is simply returned as is.
  // TODO(timvolodine): consider reducing the lux value precision further.
  return std::isinf(lux) ? lux : std::round(lux);
}
}  // namespace

namespace blink {

DeviceLightDispatcher& DeviceLightDispatcher::Instance() {
  DEFINE_STATIC_LOCAL(DeviceLightDispatcher, device_light_dispatcher,
                      (new DeviceLightDispatcher));
  return device_light_dispatcher;
}

DeviceLightDispatcher::DeviceLightDispatcher() : last_device_light_data_(-1) {}

DeviceLightDispatcher::~DeviceLightDispatcher() {}

DEFINE_TRACE(DeviceLightDispatcher) {
  PlatformEventDispatcher::Trace(visitor);
}

void DeviceLightDispatcher::StartListening() {
  Platform::Current()->StartListening(kWebPlatformEventTypeDeviceLight, this);
}

void DeviceLightDispatcher::StopListening() {
  Platform::Current()->StopListening(kWebPlatformEventTypeDeviceLight);
  last_device_light_data_ = -1;
}

void DeviceLightDispatcher::DidChangeDeviceLight(double value) {
  double newValue = EnsureRoundedLuxValue(value);
  if (last_device_light_data_ != newValue) {
    last_device_light_data_ = newValue;
    NotifyControllers();
  }
}

double DeviceLightDispatcher::LatestDeviceLightData() const {
  return last_device_light_data_;
}

}  // namespace blink
