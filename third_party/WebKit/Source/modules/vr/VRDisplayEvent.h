// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRDisplayEvent_h
#define VRDisplayEvent_h

#include "modules/EventModules.h"
#include "modules/vr/VRDisplay.h"
#include "modules/vr/VRDisplayEventInit.h"

namespace blink {

class VRDisplayEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VRDisplayEvent* Create() { return new VRDisplayEvent; }
  static VRDisplayEvent* Create(const AtomicString& type,
                                bool can_bubble,
                                bool cancelable,
                                VRDisplay* display,
                                String reason) {
    return new VRDisplayEvent(type, can_bubble, cancelable, display, reason);
  }
  static VRDisplayEvent* Create(const AtomicString& type,
                                bool can_bubble,
                                bool cancelable,
                                VRDisplay*,
                                device::mojom::blink::VRDisplayEventReason);

  static VRDisplayEvent* Create(const AtomicString& type,
                                const VRDisplayEventInit& initializer) {
    return new VRDisplayEvent(type, initializer);
  }

  ~VRDisplayEvent() override;

  VRDisplay* display() const { return display_.Get(); }
  const String& reason() const { return reason_; }

  const AtomicString& InterfaceName() const override;

  virtual void Trace(blink::Visitor*);

 private:
  VRDisplayEvent();
  VRDisplayEvent(const AtomicString& type,
                 bool can_bubble,
                 bool cancelable,
                 VRDisplay*,
                 String);
  VRDisplayEvent(const AtomicString&, const VRDisplayEventInit&);

  Member<VRDisplay> display_;
  String reason_;
};

}  // namespace blink

#endif  // VRDisplayEvent_h
