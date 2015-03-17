// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/vr/HMDVRDevice.h"

#include "modules/vr/VRFieldOfView.h"
#include "public/platform/Platform.h"
#include "public/platform/WebSize.h"

namespace blink {

HMDVRDevice::HMDVRDevice(VRHardwareUnit* hardwareUnit)
    : VRDevice(hardwareUnit)
    , m_dirtyFov(true)
{
    m_eyeParametersLeft = VREyeParameters::create();
    m_eyeParametersRight = VREyeParameters::create();
}

VREye HMDVRDevice::StringToVREye(const String& whichEye)
{
    if (whichEye == "left")
        return VREyeLeft;
    if (whichEye == "right")
        return VREyeRight;
    return VREyeNone;
}

void HMDVRDevice::updateFromWebVRDevice(const WebVRDevice& device)
{
    VRDevice::updateFromWebVRDevice(device);
    const blink::WebVRHMDInfo &hmdInfo = device.hmdInfo;

    m_eyeParametersLeft->setFromWebVREyeParameters(hmdInfo.leftEye);
    m_eyeParametersRight->setFromWebVREyeParameters(hmdInfo.rightEye);
}

VREyeParameters* HMDVRDevice::getEyeParameters(const String& whichEye)
{
    // If we have updated the field of view since the last call recompute the render rects.
    if (m_dirtyFov) {
        blink::WebVRVector4 leftRect, rightRect;
        blink::Platform::current()->getVRRenderTargetRects(index(),
            m_eyeParametersLeft->currentFieldOfView()->toWebVRFieldOfView(),
            m_eyeParametersRight->currentFieldOfView()->toWebVRFieldOfView(),
            &leftRect,
            &rightRect);

        m_eyeParametersLeft->setRenderRect(leftRect.x, leftRect.y, leftRect.z, leftRect.w);
        m_eyeParametersRight->setRenderRect(rightRect.x, rightRect.y, rightRect.z, rightRect.w);

        m_dirtyFov = false;
    }

    switch (StringToVREye(whichEye)) {
    case VREyeLeft:
        return m_eyeParametersLeft;
    case VREyeRight:
        return m_eyeParametersRight;
    default:
        return nullptr;
    }
}

void HMDVRDevice::setFieldOfView(VRFieldOfView* leftFov, VRFieldOfView* rightFov)
{
    m_dirtyFov = true;

    // FIXME: Clamp to maxFOV
    if (leftFov) {
        hardwareUnit()->setFieldOfView(VREyeLeft, leftFov);
        m_eyeParametersLeft->setCurrentFieldOfView(leftFov);
    } else {
        hardwareUnit()->setFieldOfView(VREyeLeft, m_eyeParametersLeft->recommendedFieldOfView());
        m_eyeParametersLeft->setCurrentFieldOfView(m_eyeParametersLeft->recommendedFieldOfView());
    }

    if (rightFov) {
        hardwareUnit()->setFieldOfView(VREyeRight, rightFov);
        m_eyeParametersLeft->setCurrentFieldOfView(rightFov);
    } else {
        hardwareUnit()->setFieldOfView(VREyeRight, m_eyeParametersRight->recommendedFieldOfView());
        m_eyeParametersRight->setCurrentFieldOfView(m_eyeParametersRight->recommendedFieldOfView());
    }
}

void HMDVRDevice::trace(Visitor* visitor)
{
    visitor->trace(m_eyeParametersLeft);
    visitor->trace(m_eyeParametersRight);

    VRDevice::trace(visitor);
}

} // namespace blink
