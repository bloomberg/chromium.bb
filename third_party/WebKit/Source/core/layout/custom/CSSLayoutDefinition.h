// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSLayoutDefinition_h
#define CSSLayoutDefinition_h

#include "core/CSSPropertyNames.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/bindings/TraceWrapperV8Reference.h"
#include "platform/heap/Handle.h"
#include "v8/include/v8.h"

namespace blink {

class ScriptState;

// Represents a javascript class registered on the LayoutWorkletGlobalScope by
// the author.
// https://drafts.css-houdini.org/css-layout-api/#layout-definition
class CSSLayoutDefinition final
    : public GarbageCollectedFinalized<CSSLayoutDefinition>,
      public TraceWrapperBase {
 public:
  CSSLayoutDefinition(
      ScriptState*,
      v8::Local<v8::Function> constructor,
      v8::Local<v8::Function> intrinsic_sizes,
      v8::Local<v8::Function> layout,
      const Vector<CSSPropertyID>& native_invalidation_properties,
      const Vector<AtomicString>& custom_invalidation_properties,
      const Vector<CSSPropertyID>& child_native_invalidation_properties,
      const Vector<AtomicString>& child_custom_invalidation_properties);
  virtual ~CSSLayoutDefinition();

  const Vector<CSSPropertyID>& NativeInvalidationProperties() const {
    return native_invalidation_properties_;
  }
  const Vector<AtomicString>& CustomInvalidationProperties() const {
    return custom_invalidation_properties_;
  }
  const Vector<CSSPropertyID>& ChildNativeInvalidationProperties() const {
    return child_native_invalidation_properties_;
  }
  const Vector<AtomicString>& ChildCustomInvalidationProperties() const {
    return child_custom_invalidation_properties_;
  }

  ScriptState* GetScriptState() const { return script_state_.get(); }

  v8::Local<v8::Function> IntrinsicSizesFunctionForTesting(
      v8::Isolate* isolate) {
    return intrinsic_sizes_.NewLocal(isolate);
  }

  v8::Local<v8::Function> LayoutFunctionForTesting(v8::Isolate* isolate) {
    return layout_.NewLocal(isolate);
  }

  void Trace(blink::Visitor* visitor) {}
  void TraceWrappers(const ScriptWrappableVisitor*) const override;

 private:
  scoped_refptr<ScriptState> script_state_;

  // This object keeps the class instances, constructor function, intrinsic
  // sizes function, and layout function alive. It participates in wrapper
  // tracing as it holds onto V8 wrappers.
  TraceWrapperV8Reference<v8::Function> constructor_;
  TraceWrapperV8Reference<v8::Function> intrinsic_sizes_;
  TraceWrapperV8Reference<v8::Function> layout_;

  // TODO(ikilpatrick): Once we have a LayoutObject to use this definition,
  // we'll need a map of LayoutObject to class instances. We'll also need a
  // did_call_constructor_ field to ensure that we correctly don't use a class
  // with a throwing constructor.

  Vector<CSSPropertyID> native_invalidation_properties_;
  Vector<AtomicString> custom_invalidation_properties_;
  Vector<CSSPropertyID> child_native_invalidation_properties_;
  Vector<AtomicString> child_custom_invalidation_properties_;
};

}  // namespace blink

#endif  // CSSLayoutDefinition_h
