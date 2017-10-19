// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRSessionEvent_h
#define VRSessionEvent_h

#include "modules/EventModules.h"
#include "modules/vr/latest/VRSession.h"
#include "modules/vr/latest/VRSessionEventInit.h"

namespace blink {

class VRSessionEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VRSessionEvent* Create() { return new VRSessionEvent; }
  static VRSessionEvent* Create(const AtomicString& type, VRSession* session) {
    return new VRSessionEvent(type, session);
  }

  static VRSessionEvent* Create(const AtomicString& type,
                                const VRSessionEventInit& initializer) {
    return new VRSessionEvent(type, initializer);
  }

  ~VRSessionEvent() override;

  VRSession* session() const { return session_.Get(); }

  const AtomicString& InterfaceName() const override;

  virtual void Trace(blink::Visitor*);

 private:
  VRSessionEvent();
  VRSessionEvent(const AtomicString& type, VRSession*);
  VRSessionEvent(const AtomicString& type, const VRSessionEventInit&);

  Member<VRSession> session_;
};

}  // namespace blink

#endif  // VRDisplayEvent_h
