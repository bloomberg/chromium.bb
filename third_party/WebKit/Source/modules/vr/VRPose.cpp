// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRPose.h"

namespace blink {

namespace {

DOMFloat32Array* vecToFloat32Array(const WebVRVector4& vec, bool valid)
{
    if (valid) {
        DOMFloat32Array* out = DOMFloat32Array::create(4);
        out->data()[0] = vec.x;
        out->data()[1] = vec.y;
        out->data()[2] = vec.z;
        out->data()[3] = vec.w;
        return out;
    }
    return nullptr;
}

DOMFloat32Array* vecToFloat32Array(const WebVRVector3& vec, bool valid)
{
    if (valid) {
        DOMFloat32Array* out = DOMFloat32Array::create(3);
        out->data()[0] = vec.x;
        out->data()[1] = vec.y;
        out->data()[2] = vec.z;
        return out;
    }
    return nullptr;
}

} // namespace

VRPose::VRPose()
    : m_timeStamp(0.0)
{
}

void VRPose::setPose(const WebHMDSensorState &state)
{
    m_timeStamp = state.timestamp;
    m_orientation = vecToFloat32Array(state.orientation, state.flags & WebVRSensorStateOrientation);
    m_position = vecToFloat32Array(state.position, state.flags & WebVRSensorStatePosition);
    m_angularVelocity = vecToFloat32Array(state.angularVelocity, state.flags & WebVRSensorStateAngularVelocity);
    m_linearVelocity = vecToFloat32Array(state.linearVelocity, state.flags & WebVRSensorStateLinearVelocity);
    m_angularAcceleration = vecToFloat32Array(state.angularAcceleration, state.flags & WebVRSensorStateAngularAcceleration);
    m_linearAcceleration = vecToFloat32Array(state.linearAcceleration, state.flags & WebVRSensorStateLinearAcceleration);
}

DEFINE_TRACE(VRPose)
{
    visitor->trace(m_orientation);
    visitor->trace(m_position);
    visitor->trace(m_angularVelocity);
    visitor->trace(m_linearVelocity);
    visitor->trace(m_angularAcceleration);
    visitor->trace(m_linearAcceleration);
}

} // namespace blink
