// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_unbounded_reference_space.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRUnboundedReferenceSpace::XRUnboundedReferenceSpace(XRSession* session)
    : XRReferenceSpace(session) {}

XRUnboundedReferenceSpace::~XRUnboundedReferenceSpace() = default;

std::unique_ptr<TransformationMatrix>
XRUnboundedReferenceSpace::TransformBasePose(
    const TransformationMatrix& base_pose) {
  // For now we assume that poses returned by systems that support unbounded
  // reference spaces are already in the correct space.
  return TransformationMatrix::Create(base_pose);
}

// Serves the same purpose as TransformBasePose, but for input poses. Needs to
// know the head pose so that cases like the head-model frame of reference can
// properly adjust the input's relative position.
std::unique_ptr<TransformationMatrix>
XRUnboundedReferenceSpace::TransformBaseInputPose(
    const TransformationMatrix& base_input_pose,
    const TransformationMatrix& base_pose) {
  return TransformBasePose(base_input_pose);
}

void XRUnboundedReferenceSpace::Trace(blink::Visitor* visitor) {
  XRReferenceSpace::Trace(visitor);
}

}  // namespace blink
