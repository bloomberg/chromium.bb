// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WORLD_TRACKING_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WORLD_TRACKING_STATE_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class XRPlaneDetectionState;

class XRWorldTrackingState : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRWorldTrackingState(bool plane_detection_enabled);

  XRPlaneDetectionState* planeDetectionState() const {
    return plane_detection_state_;
  }

  void Trace(Visitor* visitor) const override;

 private:
  Member<XRPlaneDetectionState> plane_detection_state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WORLD_TRACKING_STATE_H_
