// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8ThrowDOMException.h"

#include "bindings/core/v8/ToV8ForCore.h"
#include "core/dom/DOMException.h"
#include "platform/bindings/V8PrivateProperty.h"
#include "platform/bindings/V8ThrowException.h"

namespace blink {

namespace {

void DomExceptionStackGetter(v8::Local<v8::Name> name,
                             const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Value> value;
  if (info.Data()
          .As<v8::Object>()
          ->Get(isolate->GetCurrentContext(), V8AtomicString(isolate, "stack"))
          .ToLocal(&value))
    V8SetReturnValue(info, value);
}

void DomExceptionStackSetter(v8::Local<v8::Name> name,
                             v8::Local<v8::Value> value,
                             const v8::PropertyCallbackInfo<void>& info) {
  v8::Maybe<bool> unused = info.Data().As<v8::Object>()->Set(
      info.GetIsolate()->GetCurrentContext(),
      V8AtomicString(info.GetIsolate(), "stack"), value);
  ALLOW_UNUSED_LOCAL(unused);
}

}  // namespace

v8::Local<v8::Value> V8ThrowDOMException::CreateDOMException(
    v8::Isolate* isolate,
    ExceptionCode exception_code,
    const String& sanitized_message,
    const String& unsanitized_message) {
  DCHECK_GT(exception_code, 0);
  DCHECK(exception_code == kSecurityError || unsanitized_message.IsNull());

  if (isolate->IsExecutionTerminating())
    return v8::Local<v8::Value>();

  switch (exception_code) {
    case kV8Error:
      return V8ThrowException::CreateError(isolate, sanitized_message);
    case kV8TypeError:
      return V8ThrowException::CreateTypeError(isolate, sanitized_message);
    case kV8RangeError:
      return V8ThrowException::CreateRangeError(isolate, sanitized_message);
    case kV8SyntaxError:
      return V8ThrowException::CreateSyntaxError(isolate, sanitized_message);
    case kV8ReferenceError:
      return V8ThrowException::CreateReferenceError(isolate, sanitized_message);
  }

  DOMException* dom_exception = DOMException::Create(
      exception_code, sanitized_message, unsanitized_message);
  v8::Local<v8::Object> exception_obj =
      ToV8(dom_exception, isolate->GetCurrentContext()->Global(), isolate)
          .As<v8::Object>();
  // Attach an Error object to the DOMException. This is then lazily used to
  // get the stack value.
  v8::Local<v8::Value> error =
      v8::Exception::Error(V8String(isolate, dom_exception->message()));
  exception_obj
      ->SetAccessor(isolate->GetCurrentContext(),
                    V8AtomicString(isolate, "stack"), DomExceptionStackGetter,
                    DomExceptionStackSetter, error)
      .ToChecked();

  auto private_error = V8PrivateProperty::GetDOMExceptionError(isolate);
  private_error.Set(exception_obj, error);

  return exception_obj;
}

void V8ThrowDOMException::ThrowDOMException(v8::Isolate* isolate,
                                            ExceptionCode exception_code,
                                            const String& sanitized_message,
                                            const String& unsanitized_message) {
  v8::Local<v8::Value> dom_exception = CreateDOMException(
      isolate, exception_code, sanitized_message, unsanitized_message);
  if (dom_exception.IsEmpty())
    return;
  V8ThrowException::ThrowException(isolate, dom_exception);
}

}  // namespace blink
