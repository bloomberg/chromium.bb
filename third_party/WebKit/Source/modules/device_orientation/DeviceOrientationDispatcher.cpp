/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/device_orientation/DeviceOrientationDispatcher.h"

#include "modules/device_orientation/DeviceOrientationController.h"
#include "modules/device_orientation/DeviceOrientationData.h"
#include "public/platform/Platform.h"

namespace blink {

DeviceOrientationDispatcher& DeviceOrientationDispatcher::Instance(
    bool absolute) {
  if (absolute) {
    DEFINE_STATIC_LOCAL(DeviceOrientationDispatcher,
                        device_orientation_absolute_dispatcher,
                        (new DeviceOrientationDispatcher(absolute)));
    return device_orientation_absolute_dispatcher;
  }
  DEFINE_STATIC_LOCAL(DeviceOrientationDispatcher,
                      device_orientation_dispatcher,
                      (new DeviceOrientationDispatcher(absolute)));
  return device_orientation_dispatcher;
}

DeviceOrientationDispatcher::DeviceOrientationDispatcher(bool absolute)
    : absolute_(absolute) {}

DeviceOrientationDispatcher::~DeviceOrientationDispatcher() {}

void DeviceOrientationDispatcher::Trace(blink::Visitor* visitor) {
  visitor->Trace(last_device_orientation_data_);
  PlatformEventDispatcher::Trace(visitor);
}

void DeviceOrientationDispatcher::StartListening() {
  Platform::Current()->StartListening(GetWebPlatformEventType(), this);
}

void DeviceOrientationDispatcher::StopListening() {
  Platform::Current()->StopListening(GetWebPlatformEventType());
  last_device_orientation_data_.Clear();
}

void DeviceOrientationDispatcher::DidChangeDeviceOrientation(
    const device::OrientationData& motion) {
  last_device_orientation_data_ = DeviceOrientationData::Create(motion);
  NotifyControllers();
}

DeviceOrientationData*
DeviceOrientationDispatcher::LatestDeviceOrientationData() {
  return last_device_orientation_data_.Get();
}

WebPlatformEventType DeviceOrientationDispatcher::GetWebPlatformEventType()
    const {
  return (absolute_) ? kWebPlatformEventTypeDeviceOrientationAbsolute
                     : kWebPlatformEventTypeDeviceOrientation;
}

}  // namespace blink
