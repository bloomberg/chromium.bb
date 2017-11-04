/*
 * Copyright (C) 2007, 2008, 2009 Google Inc. All rights reserved.
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

#include "bindings/core/v8/V8Document.h"

#include <memory>
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8EventTarget.h"
#include "bindings/core/v8/V8HTMLAllCollection.h"
#include "bindings/core/v8/V8HTMLCollection.h"
#include "bindings/core/v8/V8Node.h"
#include "bindings/core/v8/V8Window.h"
#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLAllCollection.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLIFrameElement.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/StdLibExtras.h"

namespace blink {

// HTMLDocument ----------------------------------------------------------------

void V8Document::openMethodCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  Document* document = V8Document::ToImpl(info.Holder());

  if (info.Length() > 2) {
    if (!document->domWindow()) {
      ExceptionState exception_state(info.GetIsolate(),
                                     ExceptionState::kExecutionContext,
                                     "Document", "open");
      exception_state.ThrowDOMException(kInvalidAccessError,
                                        "document.open should throw when it "
                                        "has no window and is called with "
                                        "three arguments");
      return;
    }
    LocalFrame* frame = document->GetFrame();
    if (!frame)
      return;
    // Fetch the global object for the frame.
    v8::Local<v8::Context> context =
        ToV8Context(frame, DOMWrapperWorld::Current(info.GetIsolate()));
    // Bail out if we cannot get the context.
    if (context.IsEmpty())
      return;
    v8::Local<v8::Object> global = context->Global();
    // Get the open property of the global object.
    v8::Local<v8::Value> function =
        global->Get(V8AtomicString(info.GetIsolate(), "open"));
    // Failed; return without throwing (new) exception.
    if (function.IsEmpty())
      return;
    // If the open property is not a function throw a type error.
    if (!function->IsFunction()) {
      V8ThrowException::ThrowTypeError(info.GetIsolate(),
                                       "open is not a function");
      return;
    }
    // Wrap up the arguments and call the function.
    std::unique_ptr<v8::Local<v8::Value>[]> params =
        WrapArrayUnique(new v8::Local<v8::Value>[info.Length()]);
    for (int i = 0; i < info.Length(); i++)
      params[i] = info[i];

    V8SetReturnValue(
        info, V8ScriptRunner::CallFunction(
                  v8::Local<v8::Function>::Cast(function), frame->GetDocument(),
                  global, info.Length(), params.get(), info.GetIsolate()));
    return;
  }

  ExceptionState exception_state(
      info.GetIsolate(), ExceptionState::kExecutionContext, "Document", "open");
  document->open(EnteredDOMWindow(info.GetIsolate())->document(),
                 exception_state);

  V8SetReturnValue(info, info.Holder());
}

void V8Document::createTouchMethodPrologueCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    Document*) {
  v8::Local<v8::Value> v8_window = info[0];
  if (IsUndefinedOrNull(v8_window)) {
    UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                      WebFeature::kDocumentCreateTouchWindowNull);
  } else if (!ToDOMWindow(info.GetIsolate(), v8_window)) {
    UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                      WebFeature::kDocumentCreateTouchWindowWrongType);
  }

  v8::Local<v8::Value> v8_target = info[1];
  if (IsUndefinedOrNull(v8_target)) {
    UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                      WebFeature::kDocumentCreateTouchTargetNull);
  } else if (!V8EventTarget::hasInstance(v8_target, info.GetIsolate())) {
    UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                      WebFeature::kDocumentCreateTouchTargetWrongType);
  }
}

}  // namespace blink
