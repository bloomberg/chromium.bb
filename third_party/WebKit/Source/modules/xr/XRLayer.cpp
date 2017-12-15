// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/xr/XRLayer.h"
#include "modules/xr/XRSession.h"

namespace blink {

XRLayer::XRLayer(XRSession* session, XRLayerType layer_type)
    : session_(session), layer_type_(layer_type) {}

XRViewport* XRLayer::GetViewport(XRView::Eye) {
  return nullptr;
}

void XRLayer::OnFrameStart() {}
void XRLayer::OnFrameEnd() {}
void XRLayer::OnResize() {}

void XRLayer::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
