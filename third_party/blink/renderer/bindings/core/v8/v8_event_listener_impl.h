// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_EVENT_LISTENER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_EVENT_LISTENER_IMPL_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_event_listener.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "v8/include/v8.h"

namespace blink {

class Event;

class CORE_EXPORT V8EventListenerImpl final : public EventListener {
 public:
  static V8EventListenerImpl* Create(
      v8::Local<v8::Object> listener,
      ScriptState* script_state,
      const V8PrivateProperty::Symbol& property) {
    return new V8EventListenerImpl(listener, script_state, property);
  }

  static const V8EventListenerImpl* Cast(const EventListener* listener) {
    return listener->GetType() == kJSEventListenerType
               ? static_cast<const V8EventListenerImpl*>(listener)
               : nullptr;
  }

  static V8EventListenerImpl* Cast(EventListener* listener) {
    return const_cast<V8EventListenerImpl*>(
        Cast(const_cast<const EventListener*>(listener)));
  }

  ~V8EventListenerImpl() override;

  // Check the identity of |V8EventListener::callback_object_|. There can be
  // multiple CallbackInterfaceBase objects that have the same
  // |CallbackInterfaceBase::callback_object_| but have different
  // |CallbackInterfaceBase::incumbent_script_state_|s.
  bool operator==(const EventListener& other) const override {
    const V8EventListenerImpl* other_event_listener = Cast(&other);
    if (!other_event_listener)
      return false;
    return event_listener_->HasTheSameCallbackObject(
        *other_event_listener->event_listener_);
  }

  // blink::EventListener overrides:
  v8::Local<v8::Object> GetListenerObjectForInspector(
      ExecutionContext* execution_context) override {
    return event_listener_->CallbackObject();
  }
  DOMWrapperWorld* GetWorldForInspector() const override { return &World(); }
  void handleEvent(ExecutionContext*, Event*) override;
  void Trace(blink::Visitor*) override;

  v8::Local<v8::Object> GetListenerObject() const {
    return event_listener_->CallbackObject();
  }

 private:
  V8EventListenerImpl(v8::Local<v8::Object>,
                      ScriptState*,
                      const V8PrivateProperty::Symbol&);

  DOMWrapperWorld& World() const {
    return event_listener_->CallbackRelevantScriptState()->World();
  }

  const TraceWrapperMember<V8EventListener> event_listener_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_EVENT_LISTENER_IMPL_H_
