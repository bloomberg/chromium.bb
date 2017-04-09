/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings/core/v8/V8ThrowException.h"

#include "bindings/core/v8/BindingSecurity.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMException.h"
#include "bindings/core/v8/V8PrivateProperty.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"

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

v8::Local<v8::Value> V8ThrowException::CreateDOMException(
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
      return CreateError(isolate, sanitized_message);
    case kV8TypeError:
      return CreateTypeError(isolate, sanitized_message);
    case kV8RangeError:
      return CreateRangeError(isolate, sanitized_message);
    case kV8SyntaxError:
      return CreateSyntaxError(isolate, sanitized_message);
    case kV8ReferenceError:
      return CreateReferenceError(isolate, sanitized_message);
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

#define DEFINE_CREATE_AND_THROW_ERROR_FUNC(blinkErrorType, v8ErrorType,  \
                                           defaultMessage)               \
  v8::Local<v8::Value> V8ThrowException::Create##blinkErrorType(         \
      v8::Isolate* isolate, const String& message) {                     \
    return v8::Exception::v8ErrorType(                                   \
        V8String(isolate, message.IsNull() ? defaultMessage : message)); \
  }                                                                      \
                                                                         \
  void V8ThrowException::Throw##blinkErrorType(v8::Isolate* isolate,     \
                                               const String& message) {  \
    ThrowException(isolate, Create##blinkErrorType(isolate, message));   \
  }

DEFINE_CREATE_AND_THROW_ERROR_FUNC(Error, Error, "Error")
DEFINE_CREATE_AND_THROW_ERROR_FUNC(RangeError, RangeError, "Range error")
DEFINE_CREATE_AND_THROW_ERROR_FUNC(ReferenceError,
                                   ReferenceError,
                                   "Reference error")
DEFINE_CREATE_AND_THROW_ERROR_FUNC(SyntaxError, SyntaxError, "Syntax error")
DEFINE_CREATE_AND_THROW_ERROR_FUNC(TypeError, TypeError, "Type error")

#undef DEFINE_CREATE_AND_THROW_ERROR_FUNC

}  // namespace blink
