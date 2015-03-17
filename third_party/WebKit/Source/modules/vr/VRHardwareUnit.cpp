// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/vr/VRHardwareUnit.h"

#include "modules/vr/HMDVRDevice.h"
#include "modules/vr/PositionSensorVRDevice.h"
#include "modules/vr/VRDevice.h"
#include "public/platform/Platform.h"

namespace blink {

VRHardwareUnit::VRHardwareUnit()
    : m_frameIndex(0)
{
    m_positionState = VRPositionState::create();
    m_currentFOVLeft = new VRFieldOfView();
    m_currentFOVRight = new VRFieldOfView();
}

VRHardwareUnit::~VRHardwareUnit()
{
}

void VRHardwareUnit::updateFromWebVRDevice(const WebVRDevice& device)
{
    m_index = device.index;
    m_hardwareUnitId = device.deviceId;

    if (device.flags & WebVRDeviceTypeHMD) {
        if (!m_hmd)
            m_hmd = new HMDVRDevice(this);
        m_hmd->updateFromWebVRDevice(device);
    } else if (m_hmd) {
        m_hmd.clear();
    }

    if (device.flags & WebVRDeviceTypePosition) {
        if (!m_positionSensor)
            m_positionSensor = new PositionSensorVRDevice(this);
        m_positionSensor->updateFromWebVRDevice(device);
    } else if (m_positionSensor) {
        m_positionSensor.clear();
    }
}

void VRHardwareUnit::addDevicesToVector(HeapVector<Member<VRDevice>>& vrDevices)
{
    if (m_hmd)
        vrDevices.append(m_hmd);

    if (m_positionSensor)
        vrDevices.append(m_positionSensor);
}

VRPositionState* VRHardwareUnit::getPositionState()
{
    blink::WebHMDSensorState state;
    blink::Platform::current()->getHMDSensorState(m_index, state);

    m_positionState->setState(state);
    m_frameIndex = state.frameIndex;

    return m_positionState;
}

void VRHardwareUnit::setFieldOfView(VREye eye, VRFieldOfView* fov)
{
    switch (eye) {
    case VREyeLeft:
        m_currentFOVLeft->setFromVRFieldOfView(*fov);
        break;
    case VREyeRight:
        m_currentFOVRight->setFromVRFieldOfView(*fov);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

VRFieldOfView* VRHardwareUnit::getCurrentEyeFieldOfView(VREye eye) const
{
    switch (eye) {
    case VREyeLeft:
        return new VRFieldOfView(*m_currentFOVLeft);
    case VREyeRight:
        return new VRFieldOfView(*m_currentFOVRight);
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

void VRHardwareUnit::trace(Visitor* visitor)
{
    visitor->trace(m_positionState);
    visitor->trace(m_currentFOVLeft);
    visitor->trace(m_currentFOVRight);
    visitor->trace(m_hmd);
    visitor->trace(m_positionSensor);
}

} // namespace blink
