// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VREyeParameters.h"
#include "public/platform/modules/vr/WebVR.h"

namespace blink {

namespace {

void setVecToFloat32Array(DOMFloat32Array* out, const WebVRVector3& vec)
{
    out->data()[0] = vec.x;
    out->data()[1] = vec.y;
    out->data()[2] = vec.z;
}

} // namespace

VREyeParameters::VREyeParameters()
{
    m_offset = DOMFloat32Array::create(3);
    m_fieldOfView = new VRFieldOfView();
    m_renderWidth = 0;
    m_renderHeight = 0;
}

void VREyeParameters::update(const WebVREyeParameters &eye_parameters)
{
    setVecToFloat32Array(m_offset.get(), eye_parameters.eyeTranslation);
    m_fieldOfView->setFromWebVRFieldOfView(eye_parameters.recommendedFieldOfView);
    m_renderWidth = eye_parameters.renderRect.width;
    m_renderHeight = eye_parameters.renderRect.height;
}

DEFINE_TRACE(VREyeParameters)
{
    visitor->trace(m_offset);
    visitor->trace(m_fieldOfView);
}

} // namespace blink
