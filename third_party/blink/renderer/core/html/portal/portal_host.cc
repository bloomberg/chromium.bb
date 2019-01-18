// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/portal/portal_host.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

PortalHost::PortalHost(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

void PortalHost::Trace(Visitor* visitor) {
  EventTargetWithInlineData::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

// static
const char PortalHost::kSupplementName[] = "PortalHost";

// static
PortalHost& PortalHost::From(LocalDOMWindow& window) {
  PortalHost* portal_host =
      Supplement<LocalDOMWindow>::From<PortalHost>(window);
  if (!portal_host) {
    portal_host = MakeGarbageCollected<PortalHost>(window);
    Supplement<LocalDOMWindow>::ProvideTo<PortalHost>(window, portal_host);
  }
  return *portal_host;
}

const AtomicString& PortalHost::InterfaceName() const {
  return event_target_names::kPortalHost;
}

ExecutionContext* PortalHost::GetExecutionContext() const {
  return GetSupplementable()->document();
}

}  // namespace blink
