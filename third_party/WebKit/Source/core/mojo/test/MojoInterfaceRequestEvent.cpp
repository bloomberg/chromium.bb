// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/mojo/test/MojoInterfaceRequestEvent.h"

#include "core/mojo/MojoHandle.h"
#include "core/mojo/test/MojoInterfaceRequestEventInit.h"

namespace blink {

MojoInterfaceRequestEvent::~MojoInterfaceRequestEvent() {}

void MojoInterfaceRequestEvent::Trace(blink::Visitor* visitor) {
  Event::Trace(visitor);
  visitor->Trace(handle_);
}

MojoInterfaceRequestEvent::MojoInterfaceRequestEvent(MojoHandle* handle)
    : Event(EventTypeNames::interfacerequest, false, false), handle_(handle) {}

MojoInterfaceRequestEvent::MojoInterfaceRequestEvent(
    const AtomicString& type,
    const MojoInterfaceRequestEventInit& initializer)
    : Event(type, false, false), handle_(initializer.handle()) {}

}  // namespace blink
