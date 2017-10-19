// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRSession_h
#define VRSession_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/events/EventTarget.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class VRDevice;
class VRFrameOfReferenceOptions;

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

  DEFINE_ATTRIBUTE_EVENT_LISTENER(blur);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(focus);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(resetpose);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(end);

  ScriptPromise requestFrameOfReference(ScriptState*,
                                        const String& type,
                                        const VRFrameOfReferenceOptions&);

  // Called by JavaScript to manually end the session.
  ScriptPromise end(ScriptState*);

  // Called when the session is ended, either via calling the "end" function or
  // when the presentation service connection is closed.
  void ForceEnd();

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  void OnFocus();
  void OnBlur();

  virtual void Trace(blink::Visitor*);

 private:
  const Member<VRDevice> device_;
  const bool exclusive_;

  double depth_near_ = 0.1;
  double depth_far_ = 1000.0;
  bool blurred_ = false;

  bool detached_ = false;
};

}  // namespace blink

#endif  // VRWebGLLayer_h
