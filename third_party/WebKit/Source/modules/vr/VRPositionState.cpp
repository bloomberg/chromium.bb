// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/vr/VRPositionState.h"

namespace blink {

namespace {

DOMPoint* VecToDomPoint(const blink::WebVRVector4& vec, bool valid)
{
    if (valid)
        return DOMPoint::create(vec.x, vec.y, vec.z, vec.w);
    return nullptr;
}
DOMPoint* VecToDomPoint(const blink::WebVRVector3& vec, bool valid)
{
    if (valid)
        return DOMPoint::create(vec.x, vec.y, vec.z, 0.0);
    return nullptr;
}

} // namespace

VRPositionState::VRPositionState()
    : m_timeStamp(0.0)
{
}

void VRPositionState::setState(const blink::WebHMDSensorState &state)
{
    m_timeStamp = state.timestamp;
    m_orientation = VecToDomPoint(state.orientation, state.flags & WebVRSensorStateOrientation);
    m_position = VecToDomPoint(state.position, state.flags & WebVRSensorStatePosition);
    m_angularVelocity = VecToDomPoint(state.angularVelocity, state.flags & WebVRSensorStateAngularVelocity);
    m_linearVelocity = VecToDomPoint(state.linearVelocity, state.flags & WebVRSensorStateLinearVelocity);
    m_angularAcceleration = VecToDomPoint(state.angularAcceleration, state.flags & WebVRSensorStateAngularAcceleration);
    m_linearAcceleration =  VecToDomPoint(state.linearAcceleration, state.flags & WebVRSensorStateLinearAcceleration);
}

void VRPositionState::trace(Visitor* visitor)
{
    visitor->trace(m_orientation);
    visitor->trace(m_position);
    visitor->trace(m_angularVelocity);
    visitor->trace(m_linearVelocity);
    visitor->trace(m_angularAcceleration);
    visitor->trace(m_linearAcceleration);
}

} // namespace blink
