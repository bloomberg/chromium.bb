// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRSession_h
#define VRSession_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/events/EventTarget.h"
#include "modules/vr/latest/VRFrameRequestCallbackCollection.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/geometry/DoubleSize.h"
#include "platform/heap/Handle.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/wtf/Forward.h"

namespace blink {

class V8VRFrameRequestCallback;
class VRDevice;
class VRFrameOfReferenceOptions;
class VRLayer;
class VRView;

class VRSession final : public EventTargetWithInlineData {
  DEFINE_WRAPPERTYPEINFO();

 public:
  VRSession(VRDevice*, bool exclusive);

  VRDevice* device() const { return device_; }
  bool exclusive() const { return exclusive_; }

  // Near and far depths are used when computing projection matrices for this
  // Session's views. Changes will propegate to the appropriate matrices on the
  // next frame after these values are updated.
  double depthNear() const { return depth_near_; }
  void setDepthNear(double value);
  double depthFar() const { return depth_far_; }
  void setDepthFar(double value);

  VRLayer* baseLayer() const { return base_layer_; }
  void setBaseLayer(VRLayer* value);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(blur);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(focus);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(resetpose);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(end);

  ScriptPromise requestFrameOfReference(ScriptState*,
                                        const String& type,
                                        const VRFrameOfReferenceOptions&);

  int requestFrame(V8VRFrameRequestCallback*);
  void cancelFrame(int id);

  // Called by JavaScript to manually end the session.
  ScriptPromise end(ScriptState*);

  // Called when the session is ended, either via calling the "end" function or
  // when the presentation service connection is closed.
  void ForceEnd();

  // Describes the default scalar to be applied to the ideal framebuffer
  // dimensions when the developer does not specify one. Should be a value that
  // provides a good balance between quality and performance.
  double DefaultFramebufferScale() const { return 1.0; }

  // Describes the ideal dimensions of layer framebuffers, preferrably defined
  // as the size which gives 1:1 pixel ratio at the center of the user's view.
  DoubleSize IdealFramebufferSize() const;

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  void OnFocus();
  void OnBlur();
  void OnFrame(std::unique_ptr<TransformationMatrix>);

  const HeapVector<Member<VRView>>& views();

  void Trace(blink::Visitor*) override;
  virtual void TraceWrappers(const blink::ScriptWrappableVisitor*) const;

 private:
  const Member<VRDevice> device_;
  const bool exclusive_;
  Member<VRLayer> base_layer_;
  HeapVector<Member<VRView>> views_;

  VRFrameRequestCallbackCollection callback_collection_;

  double depth_near_ = 0.1;
  double depth_far_ = 1000.0;
  bool blurred_ = false;
  bool detached_ = false;
  bool pending_frame_ = false;
  bool resolving_frame_ = false;
  bool views_dirty_ = true;
};

}  // namespace blink

#endif  // VRWebGLLayer_h
