// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/vr/VRDevice.h"

namespace blink {

VRDevice::VRDevice(VRHardwareUnit* hardwareUnit)
    : m_hardwareUnit(hardwareUnit)
{
}

void VRDevice::updateFromWebVRDevice(const WebVRDevice& device)
{
    m_deviceId = device.deviceId;
    m_deviceName = device.deviceName;
}

void VRDevice::trace(Visitor* visitor)
{
    visitor->trace(m_hardwareUnit);
}

} // namespace blink
