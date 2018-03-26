// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSLayoutDefinition_h
#define CSSLayoutDefinition_h

#include "core/css/cssom/CSSStyleValue.h"
#include "core/css_property_names.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/bindings/TraceWrapperV8Reference.h"
#include "platform/heap/Handle.h"
#include "v8/include/v8.h"

namespace blink {

class FragmentResultOptions;
class LayoutCustom;
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

  // This class represents an instance of the layout class defined by the
  // CSSLayoutDefinition.
  class Instance final : public GarbageCollectedFinalized<Instance>,
                         public TraceWrapperBase {
   public:
    Instance(CSSLayoutDefinition*, v8::Local<v8::Object> instance);

    // Runs the web developer defined layout, returns true if everything
    // succeeded, and populates the FragmentResultOptions dictionary.
    bool Layout(const LayoutCustom&, FragmentResultOptions*);

    void Trace(blink::Visitor*);
    void TraceWrappers(const ScriptWrappableVisitor*) const override;
    const char* NameInHeapSnapshot() const override {
      return "CSSLayoutDefinition::Instance";
    }

   private:
    void ReportException(ExceptionState*);

    Member<CSSLayoutDefinition> definition_;
    TraceWrapperV8Reference<v8::Object> instance_;
  };

  // Creates an instance of the web developer defined class. May return a
  // nullptr if constructing the instance failed.
  Instance* CreateInstance();

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

  v8::Local<v8::Function> LayoutFunctionForTesting(v8::Isolate* isolate) {
    return layout_.NewLocal(isolate);
  }

  void Trace(blink::Visitor* visitor) {}
  void TraceWrappers(const ScriptWrappableVisitor*) const override;
  const char* NameInHeapSnapshot() const override {
    return "CSSLayoutDefinition";
  }

 private:
  scoped_refptr<ScriptState> script_state_;

  // This object keeps the class instances, constructor function, intrinsic
  // sizes function, and layout function alive. It participates in wrapper
  // tracing as it holds onto V8 wrappers.
  TraceWrapperV8Reference<v8::Function> constructor_;
  TraceWrapperV8Reference<v8::Function> intrinsic_sizes_;
  TraceWrapperV8Reference<v8::Function> layout_;

  // If a constructor call ever fails, we'll refuse to create any more
  // instances of the web developer provided class.
  bool constructor_has_failed_;

  Vector<CSSPropertyID> native_invalidation_properties_;
  Vector<AtomicString> custom_invalidation_properties_;
  Vector<CSSPropertyID> child_native_invalidation_properties_;
  Vector<AtomicString> child_custom_invalidation_properties_;
};

}  // namespace blink

#endif  // CSSLayoutDefinition_h
