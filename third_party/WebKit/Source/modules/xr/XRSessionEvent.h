// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XRSessionEvent_h
#define XRSessionEvent_h

#include "modules/EventModules.h"
#include "modules/xr/XRSession.h"
#include "modules/xr/XRSessionEventInit.h"

namespace blink {

class XRSessionEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static XRSessionEvent* Create() { return new XRSessionEvent; }
  static XRSessionEvent* Create(const AtomicString& type, XRSession* session) {
    return new XRSessionEvent(type, session);
  }

  static XRSessionEvent* Create(const AtomicString& type,
                                const XRSessionEventInit& initializer) {
    return new XRSessionEvent(type, initializer);
  }

  ~XRSessionEvent() override;

  XRSession* session() const { return session_.Get(); }

  const AtomicString& InterfaceName() const override;

  void Trace(blink::Visitor*) override;

 private:
  XRSessionEvent();
  XRSessionEvent(const AtomicString& type, XRSession*);
  XRSessionEvent(const AtomicString& type, const XRSessionEventInit&);

  Member<XRSession> session_;
};

}  // namespace blink

#endif  // XRDisplayEvent_h
