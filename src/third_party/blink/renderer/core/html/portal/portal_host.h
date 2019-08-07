// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_HOST_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExecutionContext;
class LocalDOMWindow;
class SecurityOrigin;

class CORE_EXPORT PortalHost : public EventTargetWithInlineData,
                               public Supplement<LocalDOMWindow> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PortalHost);

 public:
  explicit PortalHost(LocalDOMWindow& window);

  void Trace(Visitor* visitor) override;

  static const char kSupplementName[];
  static PortalHost& From(LocalDOMWindow& window);

  // EventTarget overrides
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;
  PortalHost* ToPortalHost() override;

  void ReceiveMessage(BlinkTransferableMessage message,
                      scoped_refptr<const SecurityOrigin> source_origin,
                      scoped_refptr<const SecurityOrigin> target_origin);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_HOST_H_
