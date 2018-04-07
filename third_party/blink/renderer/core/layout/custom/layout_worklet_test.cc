// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/custom/layout_worklet.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_module.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/custom/css_layout_definition.h"
#include "third_party/blink/renderer/core/layout/custom/layout_worklet_global_scope.h"
#include "third_party/blink/renderer/core/layout/custom/layout_worklet_global_scope_proxy.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class LayoutWorkletTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp(IntSize());
    layout_worklet_ =
        LayoutWorklet::Create(GetDocument().domWindow()->GetFrame());
    proxy_ = layout_worklet_->CreateGlobalScope();
  }

  LayoutWorkletGlobalScopeProxy* GetProxy() {
    return LayoutWorkletGlobalScopeProxy::From(proxy_.Get());
  }

  LayoutWorkletGlobalScope* GetGlobalScope() {
    return GetProxy()->global_scope();
  }

  void Terminate() {
    proxy_->TerminateWorkletGlobalScope();
    proxy_ = nullptr;
  }

  ScriptValue EvaluateScriptModule(const String& source_code) {
    ScriptState* script_state =
        GetGlobalScope()->ScriptController()->GetScriptState();
    EXPECT_TRUE(script_state);
    ScriptState::Scope scope(script_state);

    KURL js_url("https://example.com/worklet.js");
    ScriptModule module = ScriptModule::Compile(
        script_state->GetIsolate(), source_code, js_url, js_url,
        ScriptFetchOptions(), kSharableCrossOrigin,
        TextPosition::MinimumPosition(), ASSERT_NO_EXCEPTION);
    EXPECT_FALSE(module.IsNull());

    ScriptValue exception = module.Instantiate(script_state);
    EXPECT_TRUE(exception.IsEmpty());
    return module.Evaluate(script_state);
  }

 private:
  Persistent<WorkletGlobalScopeProxy> proxy_;
  Persistent<LayoutWorklet> layout_worklet_;
};

TEST_F(LayoutWorkletTest, GarbageCollectionOfCSSLayoutDefinition) {
  EvaluateScriptModule(R"JS(
    registerLayout('foo', class {
      *intrinsicSizes() { }
      *layout() { }
    });
  )JS");

  LayoutWorkletGlobalScope* global_scope = GetGlobalScope();
  CSSLayoutDefinition* definition = global_scope->FindDefinition("foo");
  EXPECT_NE(nullptr, definition);

  v8::Isolate* isolate =
      global_scope->ScriptController()->GetScriptState()->GetIsolate();
  DCHECK(isolate);

  // Set our ScopedPersistent to the layout function, and make weak.
  ScopedPersistent<v8::Function> handle;
  {
    v8::HandleScope handle_scope(isolate);
    handle.Set(isolate, definition->LayoutFunctionForTesting(isolate));
    handle.SetPhantom();
  }
  EXPECT_FALSE(handle.IsEmpty());
  EXPECT_TRUE(handle.IsWeak());

  // Run a GC, persistent shouldn't have been collected yet.
  ThreadState::Current()->CollectAllGarbage();
  V8GCController::CollectAllGarbageForTesting(isolate);
  EXPECT_FALSE(handle.IsEmpty());

  // Delete the page & associated objects.
  Terminate();

  // Run a GC, the persistent should have been collected.
  ThreadState::Current()->CollectAllGarbage();
  V8GCController::CollectAllGarbageForTesting(isolate);
  EXPECT_TRUE(handle.IsEmpty());
}

TEST_F(LayoutWorkletTest, ParseProperties) {
  EvaluateScriptModule(R"JS(
    registerLayout('foo', class {
      static get inputProperties() { return ['--prop', 'flex-basis', 'thing'] }
      static get childInputProperties() { return ['--child-prop', 'margin-top', 'other-thing'] }
      *intrinsicSizes() { }
      *layout() { }
    });
  )JS");

  LayoutWorkletGlobalScope* global_scope = GetGlobalScope();
  CSSLayoutDefinition* definition = global_scope->FindDefinition("foo");
  EXPECT_NE(nullptr, definition);

  Vector<CSSPropertyID> native_invalidation_properties = {CSSPropertyFlexBasis};
  Vector<AtomicString> custom_invalidation_properties = {"--prop"};
  Vector<CSSPropertyID> child_native_invalidation_properties = {
      CSSPropertyMarginTop};
  Vector<AtomicString> child_custom_invalidation_properties = {"--child-prop"};

  EXPECT_EQ(native_invalidation_properties,
            definition->NativeInvalidationProperties());
  EXPECT_EQ(custom_invalidation_properties,
            definition->CustomInvalidationProperties());
  EXPECT_EQ(child_native_invalidation_properties,
            definition->ChildNativeInvalidationProperties());
  EXPECT_EQ(child_custom_invalidation_properties,
            definition->ChildCustomInvalidationProperties());
}

// TODO(ikilpatrick): Move all the tests below to wpt tests once we have the
// layout API actually have effects that we can test in script.

TEST_F(LayoutWorkletTest, RegisterLayout) {
  ScriptValue error = EvaluateScriptModule(R"JS(
    registerLayout('foo', class {
      *intrinsicSizes() { }
      *layout() { }
    });
  )JS");

  EXPECT_TRUE(error.IsEmpty());

  error = EvaluateScriptModule(R"JS(
    registerLayout('bar', class {
      static get inputProperties() { return ['--prop'] }
      static get childInputProperties() { return ['--child-prop'] }
      *intrinsicSizes() { }
      *layout() { }
    });
  )JS");

  EXPECT_TRUE(error.IsEmpty());
}

TEST_F(LayoutWorkletTest, RegisterLayout_EmptyName) {
  ScriptValue error = EvaluateScriptModule(R"JS(
    registerLayout('', class {
    });
  )JS");

  // "The empty string is not a valid name."
  EXPECT_FALSE(error.IsEmpty());
}

TEST_F(LayoutWorkletTest, RegisterLayout_Duplicate) {
  ScriptValue error = EvaluateScriptModule(R"JS(
    registerLayout('foo', class {
      *intrinsicSizes() { }
      *layout() { }
    });
    registerLayout('foo', class {
      *intrinsicSizes() { }
      *layout() { }
    });
  )JS");

  // "A class with name:'foo' is already registered."
  EXPECT_FALSE(error.IsEmpty());
}

TEST_F(LayoutWorkletTest, RegisterLayout_NoIntrinsicSizes) {
  ScriptValue error = EvaluateScriptModule(R"JS(
    registerLayout('foo', class {
    });
  )JS");

  // "The 'intrinsicSizes' property on the prototype does not exist."
  EXPECT_FALSE(error.IsEmpty());
}

TEST_F(LayoutWorkletTest, RegisterLayout_ThrowingPropertyGetter) {
  ScriptValue error = EvaluateScriptModule(R"JS(
    registerLayout('foo', class {
      static get inputProperties() { throw Error(); }
    });
  )JS");

  // "Uncaught Error"
  EXPECT_FALSE(error.IsEmpty());
}

TEST_F(LayoutWorkletTest, RegisterLayout_BadPropertyGetter) {
  ScriptValue error = EvaluateScriptModule(R"JS(
    registerLayout('foo', class {
      static get inputProperties() { return 42; }
    });
  )JS");

  // "The provided value cannot be converted to a sequence."
  EXPECT_FALSE(error.IsEmpty());
}

TEST_F(LayoutWorkletTest, RegisterLayout_NoPrototype) {
  ScriptValue error = EvaluateScriptModule(R"JS(
    const foo = function() { };
    foo.prototype = undefined;
    registerLayout('foo', foo);
  )JS");

  // "The 'prototype' object on the class does not exist."
  EXPECT_FALSE(error.IsEmpty());
}

TEST_F(LayoutWorkletTest, RegisterLayout_BadPrototype) {
  ScriptValue error = EvaluateScriptModule(R"JS(
    const foo = function() { };
    foo.prototype = 42;
    registerLayout('foo', foo);
  )JS");

  // "The 'prototype' property on the class is not an object."
  EXPECT_FALSE(error.IsEmpty());
}

TEST_F(LayoutWorkletTest, RegisterLayout_BadIntrinsicSizes) {
  ScriptValue error = EvaluateScriptModule(R"JS(
    registerLayout('foo', class {
      intrinsicSizes() { }
    });
  )JS");

  // "The 'intrinsicSizes' property on the prototype is not a generator
  // function."
  EXPECT_FALSE(error.IsEmpty());

  error = EvaluateScriptModule(R"JS(
    registerLayout('foo', class {
      get intrinsicSizes() { return 42; }
    });
  )JS");

  // "The 'intrinsicSizes' property on the prototype is not a generator
  // function."
  EXPECT_FALSE(error.IsEmpty());
}

TEST_F(LayoutWorkletTest, RegisterLayout_NoLayout) {
  ScriptValue error = EvaluateScriptModule(R"JS(
    registerLayout('foo', class {
      *intrinsicSizes() { }
    });
  )JS");

  // "The 'layout' property on the prototype does not exist."
  EXPECT_FALSE(error.IsEmpty());
}

TEST_F(LayoutWorkletTest, RegisterLayout_BadLayout) {
  ScriptValue error = EvaluateScriptModule(R"JS(
    registerLayout('foo', class {
      *intrinsicSizes() { }
      layout() { }
    });
  )JS");

  // "The 'layout' property on the prototype is not a generator function."
  EXPECT_FALSE(error.IsEmpty());

  error = EvaluateScriptModule(R"JS(
    registerLayout('foo', class {
      *intrinsicSizes() { }
      get layout() { return 42; }
    });
  )JS");

  // "The 'layout' property on the prototype is not a generator function."
  EXPECT_FALSE(error.IsEmpty());
}

}  // namespace blink
