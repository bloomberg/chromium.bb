// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_element.h"

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_html.h"
#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_processing_stack.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

// HTMLElement -----------------------------------------------------------------

void V8Element::InnerHTMLAttributeSetterCustom(
    v8::Local<v8::Value> value,
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();

  v8::Local<v8::Object> holder = info.Holder();

  Element* impl = V8Element::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope delivery_scope;

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext,
                                 "Element", "innerHTML");
  CEReactionsScope ce_reactions_scope;

  // Prepare the value to be set.
  StringOrTrustedHTML cpp_value;
  // This if statement is the only difference to the generated code and ensures
  // that only null but not undefined is treated as the empty string.
  // https://crbug.com/783916
  if (value->IsNull()) {
    cpp_value.SetString(String());
  } else {
    V8StringOrTrustedHTML::ToImpl(isolate, value, cpp_value,
                                  UnionTypeConversionMode::kNotNullable,
                                  exception_state);
  }
  if (exception_state.HadException())
    return;

  impl->setInnerHTML(cpp_value, exception_state);
}

void V8Element::OuterHTMLAttributeSetterCustom(
    v8::Local<v8::Value> value,
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();

  v8::Local<v8::Object> holder = info.Holder();

  Element* impl = V8Element::ToImpl(holder);

  V0CustomElementProcessingStack::CallbackDeliveryScope delivery_scope;

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext,
                                 "Element", "outerHTML");
  CEReactionsScope ce_reactions_scope;

  // Prepare the value to be set.
  StringOrTrustedHTML cpp_value;
  // This if statement is the only difference to the generated code and ensures
  // that only null but not undefined is treated as the empty string.
  // https://crbug.com/783916
  if (value->IsNull()) {
    cpp_value.SetString(String());
  } else {
    V8StringOrTrustedHTML::ToImpl(isolate, value, cpp_value,
                                  UnionTypeConversionMode::kNotNullable,
                                  exception_state);
  }
  if (exception_state.HadException())
    return;

  impl->setOuterHTML(cpp_value, exception_state);
}

}  // namespace blink
