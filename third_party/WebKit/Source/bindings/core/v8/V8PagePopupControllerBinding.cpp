// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8PagePopupControllerBinding.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8Window.h"
#include "core/dom/ContextFeatures.h"
#include "core/dom/Document.h"
#include "core/execution_context/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/page/PagePopupController.h"
#include "core/page/PagePopupSupplement.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

namespace {

void PagePopupControllerAttributeGetter(
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();
  DOMWindow* impl = V8Window::ToImpl(holder);
  PagePopupController* cpp_value = nullptr;
  if (LocalFrame* frame = ToLocalDOMWindow(impl)->GetFrame())
    cpp_value = PagePopupSupplement::From(*frame).GetPagePopupController();
  V8SetReturnValue(info, ToV8(cpp_value, holder, info.GetIsolate()));
}

void PagePopupControllerAttributeGetterCallback(
    v8::Local<v8::Name>,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  PagePopupControllerAttributeGetter(info);
}

}  // namespace

void V8PagePopupControllerBinding::InstallPagePopupController(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> window_wrapper) {
  ExecutionContext* execution_context =
      ToExecutionContext(window_wrapper->CreationContext());
  if (!(execution_context && execution_context->IsDocument() &&
        ContextFeatures::PagePopupEnabled(ToDocument(execution_context))))
    return;

  window_wrapper
      ->SetAccessor(
          context, V8AtomicString(context->GetIsolate(), "pagePopupController"),
          PagePopupControllerAttributeGetterCallback)
      .ToChecked();
}

}  // namespace blink
