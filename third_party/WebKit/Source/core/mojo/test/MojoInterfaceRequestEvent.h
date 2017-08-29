// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MojoInterfaceRequestEvent_h
#define MojoInterfaceRequestEvent_h

#include "core/dom/events/Event.h"

namespace blink {

class MojoHandle;
class MojoInterfaceRequestEventInit;

// An event dispatched to a MojoInterfaceInterceptor when its frame sends an
// outgoing request for the interface it was configured to intercept. The event
// includes the intercepted MojoHandle of the request pipe, which a listener may
// use to bind the request to e.g. a mock interface implementation.
class MojoInterfaceRequestEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~MojoInterfaceRequestEvent() override;

  static MojoInterfaceRequestEvent* Create(MojoHandle* handle) {
    return new MojoInterfaceRequestEvent(handle);
  }

  static MojoInterfaceRequestEvent* Create(
      const AtomicString& type,
      const MojoInterfaceRequestEventInit& initializer) {
    return new MojoInterfaceRequestEvent(type, initializer);
  }

  MojoHandle* handle() const { return handle_; }

  const AtomicString& InterfaceName() const override {
    return EventNames::MojoInterfaceRequestEvent;
  }

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit MojoInterfaceRequestEvent(MojoHandle*);
  MojoInterfaceRequestEvent(const AtomicString& type,
                            const MojoInterfaceRequestEventInit&);

  Member<MojoHandle> handle_;
};

}  // namespace blink

#endif  // MojoInterfaceRequestEvent_h
