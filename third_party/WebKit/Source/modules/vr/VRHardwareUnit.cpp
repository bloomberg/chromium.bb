// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRHardwareUnit.h"

#include "modules/vr/HMDVRDevice.h"
#include "modules/vr/NavigatorVRDevice.h"
#include "modules/vr/PositionSensorVRDevice.h"
#include "modules/vr/VRController.h"
#include "modules/vr/VRDevice.h"
#include "public/platform/Platform.h"

namespace blink {

VRHardwareUnit::VRHardwareUnit(NavigatorVRDevice* navigatorVRDevice)
    : m_nextDeviceId(1)
    , m_frameIndex(0)
    , m_navigatorVRDevice(navigatorVRDevice)
    , m_canUpdatePositionState(true)
{
    m_positionState = VRPositionState::create();
}

VRHardwareUnit::~VRHardwareUnit()
{
    if (!m_canUpdatePositionState)
        Platform::current()->currentThread()->removeTaskObserver(this);
}

void VRHardwareUnit::updateFromWebVRDevice(const WebVRDevice& device)
{
    m_index = device.index;
    m_hardwareUnitId = String::number(device.index);

    if (device.flags & WebVRDeviceTypeHMD) {
        if (!m_hmd)
            m_hmd = new HMDVRDevice(this, m_nextDeviceId++);
        m_hmd->updateFromWebVRDevice(device);
    } else if (m_hmd) {
        m_hmd.clear();
    }

    if (device.flags & WebVRDeviceTypePosition) {
        if (!m_positionSensor)
            m_positionSensor = new PositionSensorVRDevice(this, m_nextDeviceId++);
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

VRController* VRHardwareUnit::controller()
{
    return m_navigatorVRDevice->controller();
}

VRPositionState* VRHardwareUnit::getSensorState()
{
    if (m_canUpdatePositionState) {
        m_positionState = getImmediateSensorState(true);
        Platform::current()->currentThread()->addTaskObserver(this);
        m_canUpdatePositionState = false;
    }

    return m_positionState;
}

VRPositionState* VRHardwareUnit::getImmediateSensorState(bool updateFrameIndex)
{
    WebHMDSensorState state;
    controller()->getSensorState(m_index, state);
    if (updateFrameIndex)
        m_frameIndex = state.frameIndex;

    VRPositionState* immediatePositionState = VRPositionState::create();
    immediatePositionState->setState(state);
    return immediatePositionState;
}


void VRHardwareUnit::didProcessTask()
{
    // State should be stable until control is returned to the user agent.
    if (!m_canUpdatePositionState) {
        Platform::current()->currentThread()->removeTaskObserver(this);
        m_canUpdatePositionState = true;
    }
}

DEFINE_TRACE(VRHardwareUnit)
{
    visitor->trace(m_navigatorVRDevice);
    visitor->trace(m_positionState);
    visitor->trace(m_hmd);
    visitor->trace(m_positionSensor);
}

} // namespace blink
