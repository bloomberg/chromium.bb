// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/AnimationTestHelper.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/animation/CSSInterpolationEnvironment.h"
#include "core/animation/CSSInterpolationTypesMap.h"
#include "core/animation/InvalidatableInterpolation.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/style/ComputedStyle.h"

namespace blink {

void SetV8ObjectPropertyAsString(v8::Isolate* isolate,
                                 v8::Local<v8::Object> object,
                                 const StringView& name,
                                 const StringView& value) {
  object
      ->Set(isolate->GetCurrentContext(), V8String(isolate, name),
            V8String(isolate, value))
      .ToChecked();
}

void SetV8ObjectPropertyAsNumber(v8::Isolate* isolate,
                                 v8::Local<v8::Object> object,
                                 const StringView& name,
                                 double value) {
  object
      ->Set(isolate->GetCurrentContext(), V8String(isolate, name),
            v8::Number::New(isolate, value))
      .ToChecked();
}

void EnsureInterpolatedValueCached(const ActiveInterpolations& interpolations,
                                   Document& document,
                                   Element* element) {
  // TODO(smcgruer): We should be able to use a saner API approach like
  // document.EnsureStyleResolver().StyleForElement(element). However that would
  // require our callers to propertly register every animation they pass in
  // here, which the current tests do not do.
  auto style = ComputedStyle::Create();
  StyleResolverState state(document, element, style.get(), style.get());
  state.SetStyle(style);
  CSSInterpolationTypesMap map(state.GetDocument().GetPropertyRegistry());
  CSSInterpolationEnvironment environment(map, state, nullptr);
  InvalidatableInterpolation::ApplyStack(interpolations, environment);
}

}  // namespace blink
