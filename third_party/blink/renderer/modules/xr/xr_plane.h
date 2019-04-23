// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_PLANE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_PLANE_H_

#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class XRPose;
class XRSpace;

class XRPlane : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRPose* getPose(XRSpace*) const { return nullptr; }

  String orientation() const { return {}; }
  HeapVector<Member<DOMPointReadOnly>> polygon() const { return {}; }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_PLANE_H_
