// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/latest/VRLayer.h"
#include "modules/vr/latest/VRSession.h"

namespace blink {

VRLayer::VRLayer(VRSession* session, VRLayerType layer_type)
    : session_(session), layer_type_(layer_type) {}

VRViewport* VRLayer::GetViewport(VRView::Eye) {
  return nullptr;
}

void VRLayer::OnFrameStart() {}
void VRLayer::OnFrameEnd() {}
void VRLayer::OnResize() {}

void VRLayer::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
