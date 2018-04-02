// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/InitializeV8ExtrasBinding.h"

#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8ExtrasTestUtils.h"
#include "core/dom/Document.h"
#include "core/frame/UseCounter.h"
#include "core/frame/WebFeature.h"
#include "platform/bindings/V8Binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// Add "binding" to the global object. Production code should never do this;
// it would be a huge security hole.
void AddExtrasBindingToGlobal(V8TestingScope* scope) {
  auto context = scope->GetContext();
  v8::Local<v8::Object> global = context->Global();
  v8::Local<v8::Object> binding = context->GetExtrasBindingObject();
  v8::Local<v8::String> key = V8AtomicString(scope->GetIsolate(), "binding");
  global->Set(context, key, binding).FromJust();
}

TEST(InitializeV8ExtrasBindingTest, SupportedId) {
  V8TestingScope scope;
  InitializeV8ExtrasBinding(scope.GetScriptState());
  AddExtrasBindingToGlobal(&scope);

  ScriptValue rv = EvalWithPrintingError(
      &scope, "binding.countUse('TransformStreamConstructor');");
  EXPECT_TRUE(rv.IsUndefined());
  EXPECT_TRUE(UseCounter::IsCounted(scope.GetDocument(),
                                    WebFeature::kTransformStreamConstructor));
}

TEST(InitializeV8ExtrasBindingTest, UnsupportedId) {
  V8TestingScope scope;
  InitializeV8ExtrasBinding(scope.GetScriptState());
  AddExtrasBindingToGlobal(&scope);

  ScriptValue rv = EvalWithPrintingError(&scope,
                                         "let result;"
                                         "try {"
                                         "  binding.countUse('WindowEvent');"
                                         "  result = 'FAIL';"
                                         "} catch (e) {"
                                         "  result = e.name;"
                                         "}"
                                         "result");
  String result;
  EXPECT_TRUE(rv.ToString(result));
  EXPECT_EQ("TypeError", result);
}

}  // namespace

}  // namespace blink
