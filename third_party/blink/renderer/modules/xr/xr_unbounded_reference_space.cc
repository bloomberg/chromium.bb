// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_unbounded_reference_space.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space_event.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRUnboundedReferenceSpace::XRUnboundedReferenceSpace(XRSession* session)
    : XRReferenceSpace(session) {}

XRUnboundedReferenceSpace::~XRUnboundedReferenceSpace() = default;

// No default pose for unbounded reference spaces.
std::unique_ptr<TransformationMatrix> XRUnboundedReferenceSpace::DefaultPose() {
  return nullptr;
}

std::unique_ptr<TransformationMatrix>
XRUnboundedReferenceSpace::TransformBasePose(
    const TransformationMatrix& base_pose) {
  // For now we assume that poses returned by systems that support unbounded
  // reference spaces are already in the correct space.
  return std::make_unique<TransformationMatrix>(base_pose);
}

void XRUnboundedReferenceSpace::Trace(blink::Visitor* visitor) {
  XRReferenceSpace::Trace(visitor);
}

void XRUnboundedReferenceSpace::OnReset() {
  DispatchEvent(*XRReferenceSpaceEvent::Create(event_type_names::kReset, this));
}

}  // namespace blink
