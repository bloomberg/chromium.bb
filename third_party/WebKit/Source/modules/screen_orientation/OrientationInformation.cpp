// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/screen_orientation/OrientationInformation.h"

#include "modules/screen_orientation/ScreenOrientation.h"

namespace WebCore {

OrientationInformation::OrientationInformation()
    : m_type(blink::WebScreenOrientationUndefined)
    , m_angle(0)
{
    ScriptWrappable::init(this);
}

bool OrientationInformation::initialized() const
{
    return m_type != blink::WebScreenOrientationUndefined;
}

String OrientationInformation::type() const
{
    return ScreenOrientation::orientationTypeToString(m_type);
}

unsigned short OrientationInformation::angle() const
{
    return m_angle;
}

void OrientationInformation::setType(blink::WebScreenOrientationType type)
{
    ASSERT(type != blink::WebScreenOrientationUndefined);
    m_type = type;
}

void OrientationInformation::setAngle(unsigned short angle)
{
    m_angle = angle;
}

} // namespace WebCore
