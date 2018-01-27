// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/custom/CSSLayoutDefinition.h"

#include <memory>
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/ExecutionContext.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8BindingMacros.h"

namespace blink {

CSSLayoutDefinition::CSSLayoutDefinition(
    ScriptState* script_state,
    v8::Local<v8::Function> constructor,
    v8::Local<v8::Function> intrinsic_sizes,
    v8::Local<v8::Function> layout,
    const Vector<CSSPropertyID>& native_invalidation_properties,
    const Vector<AtomicString>& custom_invalidation_properties,
    const Vector<CSSPropertyID>& child_native_invalidation_properties,
    const Vector<AtomicString>& child_custom_invalidation_properties)
    : script_state_(script_state),
      constructor_(script_state->GetIsolate(), constructor),
      intrinsic_sizes_(script_state->GetIsolate(), intrinsic_sizes),
      layout_(script_state->GetIsolate(), layout) {
  native_invalidation_properties_ = native_invalidation_properties;
  custom_invalidation_properties_ = custom_invalidation_properties;
  child_native_invalidation_properties_ = child_native_invalidation_properties;
  child_custom_invalidation_properties_ = child_custom_invalidation_properties;
}

CSSLayoutDefinition::~CSSLayoutDefinition() = default;

void CSSLayoutDefinition::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(constructor_.Cast<v8::Value>());
  visitor->TraceWrappers(intrinsic_sizes_.Cast<v8::Value>());
  visitor->TraceWrappers(layout_.Cast<v8::Value>());
}

}  // namespace blink
