// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/device_light/DeviceLightDispatcher.h"

#include "modules/device_light/DeviceLightController.h"
#include "public/platform/Platform.h"

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
  last_device_light_data_ = value;
  NotifyControllers();
}

double DeviceLightDispatcher::LatestDeviceLightData() const {
  return last_device_light_data_;
}

}  // namespace blink
