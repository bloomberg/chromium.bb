/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_ABSTRACT_EVENT_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_ABSTRACT_EVENT_HANDLER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "v8/include/v8.h"

namespace blink {

class Event;
class EventTarget;

// There are two kinds of event listeners: HTML or non-HMTL. onload,
// onfocus, etc (attributes) are always HTML event handler type; Event
// listeners added by Window.addEventListener or
// EventTargetNode::addEventListener are non-HTML type.
//
// Why does this matter?
// WebKit does not allow duplicated HTML event handlers of the same type,
// but ALLOWs duplicated non-HTML event handlers.
class CORE_EXPORT V8AbstractEventHandler : public EventListener {
 public:
  ~V8AbstractEventHandler() override;

  static const V8AbstractEventHandler* Cast(const EventListener* listener) {
    return listener->GetType() == kJSEventHandlerType
               ? static_cast<const V8AbstractEventHandler*>(listener)
               : nullptr;
  }

  static V8AbstractEventHandler* Cast(EventListener* listener) {
    return const_cast<V8AbstractEventHandler*>(
        Cast(const_cast<const EventListener*>(listener)));
  }

  static v8::Local<v8::Value> GetListenerOrNull(v8::Isolate*,
                                                EventTarget*,
                                                EventListener*);

  // Implementation of EventListener interface.

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  void handleEvent(ExecutionContext*, Event*) final;
  virtual void HandleEvent(ScriptState*, Event*);

  // Returns the listener object, either a function or an object, or the empty
  // handle if the user script is not compilable.  No exception will be thrown
  // even if the user script is not compilable.
  v8::Local<v8::Object> GetListenerObjectForInspector(
      ExecutionContext* execution_context) final {
    return GetListenerObjectInternal(execution_context);
  }

  v8::Local<v8::Object> GetListenerObject(ExecutionContext* execution_context) {
    return GetListenerObjectInternal(execution_context);
  }

  v8::Local<v8::Object> GetExistingListenerObject() {
    return listener_.NewLocal(isolate_);
  }

  // Provides access to the underlying handle for GC. Returned
  // value is a weak handle and so not guaranteed to stay alive.
  v8::Persistent<v8::Object>& ExistingListenerObjectPersistentHandle() {
    return listener_.Get();
  }

  bool HasExistingListenerObject() { return !listener_.IsEmpty(); }

  void ClearListenerObject();

  bool BelongsToTheCurrentWorld(ExecutionContext*) const final;

  bool IsAttribute() const final { return is_attribute_; }

  v8::Isolate* GetIsolate() const { return isolate_; }
  DOMWrapperWorld* GetWorldForInspector() const final { return world_.get(); }

  void Trace(blink::Visitor*) override;

 protected:
  V8AbstractEventHandler(v8::Isolate*, bool is_attribute, DOMWrapperWorld&);

  virtual v8::Local<v8::Object> GetListenerObjectInternal(
      ExecutionContext* execution_context) {
    return GetExistingListenerObject();
  }

  void SetListenerObject(ScriptState*,
                         v8::Local<v8::Object>,
                         const V8PrivateProperty::Symbol&);

  void InvokeEventHandler(ScriptState*, Event*, v8::Local<v8::Value>);

  // Get the receiver object to use for event listener call.
  v8::Local<v8::Object> GetReceiverObject(ScriptState*, Event*);
  DOMWrapperWorld& World() const { return *world_; }

 private:
  // This could return an empty handle and callers need to check return value.
  // We don't use v8::MaybeLocal because it can fail without exception.
  virtual v8::Local<v8::Value>
  CallListenerFunction(ScriptState*, v8::Local<v8::Value> jsevent, Event*) = 0;

  virtual bool ShouldPreventDefault(v8::Local<v8::Value> return_value, Event*);

  static void WrapperCleared(
      const v8::WeakCallbackInfo<V8AbstractEventHandler>&);

  TraceWrapperV8Reference<v8::Object> listener_;

  // true if the listener is created through a DOM attribute.
  bool is_attribute_;

  scoped_refptr<DOMWrapperWorld> world_;
  v8::Isolate* isolate_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_ABSTRACT_EVENT_HANDLER_H_
