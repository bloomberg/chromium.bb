// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_RENDER_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_RENDER_STATE_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class XRLayer;
class XRPresentationContext;
class XRRenderStateInit;

class XRRenderState : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRRenderState() = default;
  ~XRRenderState() override = default;

  // Near and far depths are used when computing projection matrices for the
  // Session's views.
  double depthNear() const { return depth_near_; }
  double depthFar() const { return depth_far_; }
  XRLayer* baseLayer() const { return base_layer_; }
  XRPresentationContext* outputContext() const { return output_context_; }

  void Update(const XRRenderStateInit* init);

  // Only used when removing an outputContext from the session because it was
  // bound to a different session.
  void removeOutputContext();

  void Trace(blink::Visitor*) override;

 private:
  double depth_near_ = 0.1;
  double depth_far_ = 1000.0;
  Member<XRLayer> base_layer_;
  Member<XRPresentationContext> output_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_RENDER_STATE_H_
