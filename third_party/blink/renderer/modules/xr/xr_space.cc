// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_space.h"

#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRSpace::XRSpace(XRSession* session) : session_(session) {}

XRSpace::~XRSpace() = default;

// If possible, get the matrix required to transform between two coordinate
// systems.
DOMFloat32Array* XRSpace::getTransformTo(XRSpace* other) const {
  if (session_ != other->session()) {
    // Cannot get relationships between coordinate systems that belong to
    // different sessions.
    return nullptr;
  }

  // TODO(bajones): Track relationship to other coordinate systems and return
  // the transforms here. In the meantime we're allowed to return null to
  // indicate that the transform between the two coordinate systems is unknown.
  return nullptr;
}

ExecutionContext* XRSpace::GetExecutionContext() const {
  return session()->GetExecutionContext();
}

const AtomicString& XRSpace::InterfaceName() const {
  return event_target_names::kXRSpace;
}

void XRSpace::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
