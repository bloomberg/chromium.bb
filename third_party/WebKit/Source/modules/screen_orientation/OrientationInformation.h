// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OrientationInformation_h
#define OrientationInformation_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "public/platform/WebScreenOrientationType.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

// OrientationInformation is the C++ counter-part of the OrientationInformation
// web interface. It is GarbageCollectedFinalized because ScriptWrappable
// requires it.
class OrientationInformation FINAL
    : public GarbageCollectedFinalized<OrientationInformation>
    , public ScriptWrappable {
public:
    OrientationInformation();
    bool initialized() const;

    String type() const;
    unsigned short angle() const;

    void setType(blink::WebScreenOrientationType);
    void setAngle(unsigned short);

    void trace(Visitor*) { }

private:
    blink::WebScreenOrientationType m_type;
    unsigned short m_angle;
};

} // namespace WebCore

#endif // OrientationInformation_h
