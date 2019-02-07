// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_render_state.h"

#include "third_party/blink/renderer/modules/xr/xr_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_render_state_init.h"

namespace blink {

void XRRenderState::Update(const XRRenderStateInit* init) {
  if (init->hasDepthNear()) {
    depth_near_ = init->depthNear();
  }
  if (init->hasDepthFar()) {
    depth_far_ = init->depthFar();
  }
  if (init->hasBaseLayer()) {
    base_layer_ = init->baseLayer();
  }
}

void XRRenderState::Trace(blink::Visitor* visitor) {
  visitor->Trace(base_layer_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
