// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRStageParameters_h
#define VRStageParameters_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMTypedArray.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/vr/WebVR.h"
#include "wtf/Forward.h"

namespace blink {

class VRStageParameters final : public GarbageCollectedFinalized<VRStageParameters>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    VRStageParameters();

    DOMFloat32Array* sittingToStandingTransform() const { return m_standingTransform; }

    float sizeX() const { return m_sizeX; }
    float sizeZ() const { return m_sizeZ; }

    DECLARE_VIRTUAL_TRACE()

private:
    Member<DOMFloat32Array> m_standingTransform;
    float m_sizeX;
    float m_sizeZ;
};

} // namespace blink

#endif // VRStageParameters_h
