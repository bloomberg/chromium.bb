// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/portal/portal_host.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/user_activation.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/page/page.h"
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

PortalHost* PortalHost::ToPortalHost() {
  return this;
}

void PortalHost::ReceiveMessage(
    BlinkTransferableMessage message,
    scoped_refptr<const SecurityOrigin> source_origin,
    scoped_refptr<const SecurityOrigin> target_origin) {
  DCHECK(GetSupplementable()->document()->GetPage()->InsidePortal());
  if (target_origin && !target_origin->IsSameSchemeHostPort(
                           GetExecutionContext()->GetSecurityOrigin())) {
    return;
  }

  UserActivation* user_activation = nullptr;
  if (message.user_activation) {
    user_activation = MakeGarbageCollected<UserActivation>(
        message.user_activation->has_been_active,
        message.user_activation->was_active);
  }

  MessageEvent* event = MessageEvent::Create(message.ports, message.message,
                                             source_origin->ToString(),
                                             String(), this, user_activation);
  event->EntangleMessagePorts(GetExecutionContext());

  ThreadDebugger* debugger = MainThreadDebugger::Instance();
  if (debugger)
    debugger->ExternalAsyncTaskStarted(message.sender_stack_trace_id);
  DispatchEvent(*event);
  if (debugger)
    debugger->ExternalAsyncTaskFinished(message.sender_stack_trace_id);
}

}  // namespace blink
