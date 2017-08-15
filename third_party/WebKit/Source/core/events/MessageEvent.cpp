/*
 * Copyright (C) 2007 Henry Mason (hmason@mac.com)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "core/events/MessageEvent.h"

#include <memory>
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "platform/bindings/V8PrivateProperty.h"

namespace blink {

static inline bool IsValidSource(EventTarget* source) {
  return !source || source->ToLocalDOMWindow() || source->ToMessagePort() ||
         source->ToServiceWorker();
}

MessageEvent::MessageEvent() : data_type_(kDataTypeScriptValue) {}

MessageEvent::MessageEvent(const AtomicString& type,
                           const MessageEventInit& initializer)
    : Event(type, initializer),
      data_type_(kDataTypeScriptValue),
      source_(nullptr) {
  if (initializer.hasData())
    data_as_script_value_ = initializer.data();
  if (initializer.hasOrigin())
    origin_ = initializer.origin();
  if (initializer.hasLastEventId())
    last_event_id_ = initializer.lastEventId();
  if (initializer.hasSource() && IsValidSource(initializer.source()))
    source_ = initializer.source();
  if (initializer.hasPorts())
    ports_ = new MessagePortArray(initializer.ports());
  DCHECK(IsValidSource(source_.Get()));
}

MessageEvent::MessageEvent(const String& origin,
                           const String& last_event_id,
                           EventTarget* source,
                           MessagePortArray* ports,
                           const String& suborigin)
    : Event(EventTypeNames::message, false, false),
      data_type_(kDataTypeScriptValue),
      origin_(origin),
      last_event_id_(last_event_id),
      source_(source),
      ports_(ports) {
  DCHECK(IsValidSource(source_.Get()));
}

MessageEvent::MessageEvent(RefPtr<SerializedScriptValue> data,
                           const String& origin,
                           const String& last_event_id,
                           EventTarget* source,
                           MessagePortArray* ports,
                           const String& suborigin)
    : Event(EventTypeNames::message, false, false),
      data_type_(kDataTypeSerializedScriptValue),
      data_as_serialized_script_value_(
          SerializedScriptValue::Unpack(std::move(data))),
      origin_(origin),
      last_event_id_(last_event_id),
      source_(source),
      ports_(ports) {
  if (data_as_serialized_script_value_)
    data_as_serialized_script_value_->Value()
        ->RegisterMemoryAllocatedWithCurrentScriptContext();
  DCHECK(IsValidSource(source_.Get()));
}

MessageEvent::MessageEvent(RefPtr<SerializedScriptValue> data,
                           const String& origin,
                           const String& last_event_id,
                           EventTarget* source,
                           MessagePortChannelArray channels,
                           const String& suborigin)
    : Event(EventTypeNames::message, false, false),
      data_type_(kDataTypeSerializedScriptValue),
      data_as_serialized_script_value_(
          SerializedScriptValue::Unpack(std::move(data))),
      origin_(origin),
      last_event_id_(last_event_id),
      source_(source),
      channels_(std::move(channels)),
      suborigin_(suborigin) {
  if (data_as_serialized_script_value_)
    data_as_serialized_script_value_->Value()
        ->RegisterMemoryAllocatedWithCurrentScriptContext();
  DCHECK(IsValidSource(source_.Get()));
}

MessageEvent::MessageEvent(const String& data,
                           const String& origin,
                           const String& suborigin)
    : Event(EventTypeNames::message, false, false),
      data_type_(kDataTypeString),
      data_as_string_(data),
      origin_(origin) {}

MessageEvent::MessageEvent(Blob* data,
                           const String& origin,
                           const String& suborigin)
    : Event(EventTypeNames::message, false, false),
      data_type_(kDataTypeBlob),
      data_as_blob_(data),
      origin_(origin) {}

MessageEvent::MessageEvent(DOMArrayBuffer* data,
                           const String& origin,
                           const String& suborigin)
    : Event(EventTypeNames::message, false, false),
      data_type_(kDataTypeArrayBuffer),
      data_as_array_buffer_(data),
      origin_(origin) {}

MessageEvent::~MessageEvent() {}

MessageEvent* MessageEvent::Create(const AtomicString& type,
                                   const MessageEventInit& initializer,
                                   ExceptionState& exception_state) {
  if (initializer.source() && !IsValidSource(initializer.source())) {
    exception_state.ThrowTypeError(
        "The optional 'source' property is neither a Window nor MessagePort.");
    return nullptr;
  }
  return new MessageEvent(type, initializer);
}

void MessageEvent::initMessageEvent(const AtomicString& type,
                                    bool can_bubble,
                                    bool cancelable,
                                    ScriptValue data,
                                    const String& origin,
                                    const String& last_event_id,
                                    EventTarget* source,
                                    MessagePortArray* ports) {
  if (IsBeingDispatched())
    return;

  initEvent(type, can_bubble, cancelable);

  data_type_ = kDataTypeScriptValue;
  data_as_script_value_ = data;
  origin_ = origin;
  last_event_id_ = last_event_id;
  source_ = source;
  ports_ = ports;
  is_ports_dirty_ = true;
  suborigin_ = "";
}

void MessageEvent::initMessageEvent(const AtomicString& type,
                                    bool can_bubble,
                                    bool cancelable,
                                    PassRefPtr<SerializedScriptValue> data,
                                    const String& origin,
                                    const String& last_event_id,
                                    EventTarget* source,
                                    MessagePortArray* ports) {
  if (IsBeingDispatched())
    return;

  initEvent(type, can_bubble, cancelable);

  data_type_ = kDataTypeSerializedScriptValue;
  data_as_serialized_script_value_ =
      SerializedScriptValue::Unpack(std::move(data));
  origin_ = origin;
  last_event_id_ = last_event_id;
  source_ = source;
  ports_ = ports;
  is_ports_dirty_ = true;
  suborigin_ = "";

  if (data_as_serialized_script_value_)
    data_as_serialized_script_value_->Value()
        ->RegisterMemoryAllocatedWithCurrentScriptContext();
}

void MessageEvent::initMessageEvent(const AtomicString& type,
                                    bool can_bubble,
                                    bool cancelable,
                                    const String& data,
                                    const String& origin,
                                    const String& last_event_id,
                                    EventTarget* source,
                                    MessagePortArray* ports) {
  if (IsBeingDispatched())
    return;

  initEvent(type, can_bubble, cancelable);

  data_type_ = kDataTypeString;
  data_as_string_ = data;
  origin_ = origin;
  last_event_id_ = last_event_id;
  source_ = source;
  ports_ = ports;
  is_ports_dirty_ = true;
  suborigin_ = "";
}

const AtomicString& MessageEvent::InterfaceName() const {
  return EventNames::MessageEvent;
}

MessagePortArray MessageEvent::ports() {
  // TODO(bashi): Currently we return a copied array because the binding
  // layer could modify the content of the array while executing JS callbacks.
  // Avoid copying once we can make sure that the binding layer won't
  // modify the content.
  is_ports_dirty_ = false;
  return ports_ ? *ports_ : MessagePortArray();
}

void MessageEvent::EntangleMessagePorts(ExecutionContext* context) {
  ports_ = MessagePort::EntanglePorts(*context, std::move(channels_));
  is_ports_dirty_ = true;
}

DEFINE_TRACE(MessageEvent) {
  visitor->Trace(data_as_serialized_script_value_);
  visitor->Trace(data_as_blob_);
  visitor->Trace(data_as_array_buffer_);
  visitor->Trace(source_);
  visitor->Trace(ports_);
  Event::Trace(visitor);
}

v8::Local<v8::Object> MessageEvent::AssociateWithWrapper(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type,
    v8::Local<v8::Object> wrapper) {
  wrapper = Event::AssociateWithWrapper(isolate, wrapper_type, wrapper);

  // Ensures a wrapper is created for the data to return now so that V8 knows
  // how much memory is used via the wrapper. To keep the wrapper alive, it's
  // set to the wrapper of the MessageEvent as a private value.
  switch (GetDataType()) {
    case MessageEvent::kDataTypeScriptValue:
    case MessageEvent::kDataTypeSerializedScriptValue:
      break;
    case MessageEvent::kDataTypeString:
      V8PrivateProperty::GetMessageEventCachedData(isolate).Set(
          wrapper, V8String(isolate, DataAsString()));
      break;
    case MessageEvent::kDataTypeBlob:
      break;
    case MessageEvent::kDataTypeArrayBuffer:
      V8PrivateProperty::GetMessageEventCachedData(isolate).Set(
          wrapper, ToV8(DataAsArrayBuffer(), wrapper, isolate));
      break;
  }

  return wrapper;
}

}  // namespace blink
