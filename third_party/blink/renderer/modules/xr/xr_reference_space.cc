// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_stage_bounds.h"

namespace blink {

XRReferenceSpace::XRReferenceSpace(XRSession* session) : XRSpace(session) {}

XRReferenceSpace::~XRReferenceSpace() = default;

void XRReferenceSpace::Trace(blink::Visitor* visitor) {
  XRSpace::Trace(visitor);
}

}  // namespace blink
