// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VREyeParameters.h"

namespace blink {

VREyeParameters::VREyeParameters()
{
    m_offset = DOMFloat32Array::create(3);
    m_fieldOfView = new VRFieldOfView();
    m_renderWidth = 0;
    m_renderHeight = 0;
}

void VREyeParameters::update(const mojom::blink::VREyeParametersPtr& eyeParameters)
{
    m_offset->data()[0] = eyeParameters->eyeTranslation->x;
    m_offset->data()[1] = eyeParameters->eyeTranslation->y;
    m_offset->data()[2] = eyeParameters->eyeTranslation->z;

    m_fieldOfView->setUpDegrees(eyeParameters->recommendedFieldOfView->upDegrees);
    m_fieldOfView->setDownDegrees(eyeParameters->recommendedFieldOfView->downDegrees);
    m_fieldOfView->setLeftDegrees(eyeParameters->recommendedFieldOfView->leftDegrees);
    m_fieldOfView->setRightDegrees(eyeParameters->recommendedFieldOfView->rightDegrees);

    m_renderWidth = eyeParameters->renderRect->width;
    m_renderHeight = eyeParameters->renderRect->height;
}

DEFINE_TRACE(VREyeParameters)
{
    visitor->trace(m_offset);
    visitor->trace(m_fieldOfView);
}

} // namespace blink
