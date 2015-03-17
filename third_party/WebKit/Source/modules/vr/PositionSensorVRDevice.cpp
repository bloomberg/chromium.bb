// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/vr/PositionSensorVRDevice.h"

#include "public/platform/Platform.h"

namespace blink {

PositionSensorVRDevice::PositionSensorVRDevice(VRHardwareUnit* hardwareUnit)
    : VRDevice(hardwareUnit)
{
}

VRPositionState* PositionSensorVRDevice::getState()
{
    // FIXME: This value should be stable for the duration of a requestAnimationFrame callback
    return hardwareUnit()->getPositionState();
}

VRPositionState* PositionSensorVRDevice::getImmediateState()
{
    return hardwareUnit()->getPositionState();
}

void PositionSensorVRDevice::resetSensor()
{
    blink::Platform::current()->resetVRSensor(index());
}

void PositionSensorVRDevice::trace(Visitor* visitor)
{
    VRDevice::trace(visitor);
}

} // namespace blink
