// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_PORTAL_ACTIVATE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_PORTAL_ACTIVATE_EVENT_H_

#include "base/unguessable_token.h"
#include "third_party/blink/public/mojom/portal/portal.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/unpacked_serialized_script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Document;
class HTMLPortalElement;
class LocalFrame;
class ScriptState;
class ScriptValue;

class CORE_EXPORT PortalActivateEvent : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PortalActivateEvent* Create(
      LocalFrame* frame,
      const base::UnguessableToken& predecessor_portal_token,
      mojom::blink::PortalAssociatedPtr predecessor_portal_ptr,
      scoped_refptr<SerializedScriptValue> data,
      MessagePortArray* ports);

  PortalActivateEvent(Document* document,
                      const base::UnguessableToken& predecessor_portal_token,
                      mojom::blink::PortalAssociatedPtr predecessor_portal_ptr,
                      UnpackedSerializedScriptValue* data,
                      MessagePortArray*);
  ~PortalActivateEvent() override;

  void Trace(blink::Visitor*) override;

  // Event overrides
  const AtomicString& InterfaceName() const override;

  // IDL implementation.
  ScriptValue data(ScriptState*);
  HTMLPortalElement* adoptPredecessor();

 private:
  Member<Document> document_;
  base::UnguessableToken predecessor_portal_token_;
  mojom::blink::PortalAssociatedPtr predecessor_portal_ptr_;
  Member<UnpackedSerializedScriptValue> data_;
  Member<MessagePortArray> ports_;
  HeapHashMap<WeakMember<ScriptState>, TraceWrapperV8Reference<v8::Value>>
      v8_data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_PORTAL_ACTIVATE_EVENT_H_
