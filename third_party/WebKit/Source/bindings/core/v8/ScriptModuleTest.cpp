// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptModule.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/ScriptModuleResolver.h"
#include "core/frame/LocalFrame.h"
#include "core/testing/DummyModulator.h"
#include "platform/bindings/V8Binding.h"
#include "platform/bindings/V8PerContextData.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class TestScriptModuleResolver final : public ScriptModuleResolver {
 public:
  TestScriptModuleResolver() {}
  virtual ~TestScriptModuleResolver() {}

  size_t ResolveCount() const { return specifiers_.size(); }
  const Vector<String>& Specifiers() const { return specifiers_; }
  void PushScriptModule(ScriptModule script_module) {
    script_modules_.push_back(script_module);
  }

 private:
  // Implements ScriptModuleResolver:

  void RegisterModuleScript(ModuleScript*) override { NOTREACHED(); }
  void UnregisterModuleScript(ModuleScript*) override { NOTREACHED(); }

  ScriptModule Resolve(const String& specifier,
                       const ScriptModule&,
                       ExceptionState&) override {
    specifiers_.push_back(specifier);
    return script_modules_.TakeFirst();
  }

  Vector<String> specifiers_;
  Deque<ScriptModule> script_modules_;
};

class ScriptModuleTestModulator final : public DummyModulator {
 public:
  ScriptModuleTestModulator();
  virtual ~ScriptModuleTestModulator() {}

  DECLARE_TRACE();

  TestScriptModuleResolver* GetTestScriptModuleResolver() {
    return resolver_.Get();
  }

 private:
  // Implements Modulator:

  ScriptModuleResolver* GetScriptModuleResolver() override {
    return resolver_.Get();
  }

  Member<TestScriptModuleResolver> resolver_;
};

ScriptModuleTestModulator::ScriptModuleTestModulator()
    : resolver_(new TestScriptModuleResolver) {}

DEFINE_TRACE(ScriptModuleTestModulator) {
  visitor->Trace(resolver_);
  DummyModulator::Trace(visitor);
}

TEST(ScriptModuleTest, compileSuccess) {
  V8TestingScope scope;
  ScriptModule module = ScriptModule::Compile(
      scope.GetIsolate(), "export const a = 42;", "foo.js",
      kSharableCrossOrigin, TextPosition::MinimumPosition(),
      ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module.IsNull());
  EXPECT_EQ(ScriptModuleState::kUninstantiated,
            module.Status(scope.GetScriptState()));
}

TEST(ScriptModuleTest, compileFail) {
  V8TestingScope scope;
  ScriptModule module = ScriptModule::Compile(
      scope.GetIsolate(), "123 = 456", "foo.js", kSharableCrossOrigin,
      TextPosition::MinimumPosition(), scope.GetExceptionState());
  ASSERT_TRUE(module.IsNull());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

TEST(ScriptModuleTest, equalAndHash) {
  V8TestingScope scope;

  ScriptModule module_null;
  ScriptModule module_a = ScriptModule::Compile(
      scope.GetIsolate(), "export const a = 'a';", "a.js", kSharableCrossOrigin,
      TextPosition::MinimumPosition(), ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module_a.IsNull());
  ScriptModule module_b = ScriptModule::Compile(
      scope.GetIsolate(), "export const b = 'b';", "b.js", kSharableCrossOrigin,
      TextPosition::MinimumPosition(), ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module_b.IsNull());
  Vector<char> module_deleted_buffer(sizeof(ScriptModule));
  ScriptModule& module_deleted =
      *reinterpret_cast<ScriptModule*>(module_deleted_buffer.data());
  HashTraits<ScriptModule>::ConstructDeletedValue(module_deleted, true);

  EXPECT_EQ(module_null, module_null);
  EXPECT_EQ(module_a, module_a);
  EXPECT_EQ(module_b, module_b);
  EXPECT_EQ(module_deleted, module_deleted);

  EXPECT_NE(module_null, module_a);
  EXPECT_NE(module_null, module_b);
  EXPECT_NE(module_null, module_deleted);

  EXPECT_NE(module_a, module_null);
  EXPECT_NE(module_a, module_b);
  EXPECT_NE(module_a, module_deleted);

  EXPECT_NE(module_b, module_null);
  EXPECT_NE(module_b, module_a);
  EXPECT_NE(module_b, module_deleted);

  EXPECT_NE(module_deleted, module_null);
  EXPECT_NE(module_deleted, module_a);
  EXPECT_NE(module_deleted, module_b);

  EXPECT_NE(DefaultHash<ScriptModule>::Hash::GetHash(module_a),
            DefaultHash<ScriptModule>::Hash::GetHash(module_b));
  EXPECT_NE(DefaultHash<ScriptModule>::Hash::GetHash(module_null),
            DefaultHash<ScriptModule>::Hash::GetHash(module_a));
  EXPECT_NE(DefaultHash<ScriptModule>::Hash::GetHash(module_null),
            DefaultHash<ScriptModule>::Hash::GetHash(module_b));
}

TEST(ScriptModuleTest, moduleRequests) {
  V8TestingScope scope;
  ScriptModule module = ScriptModule::Compile(
      scope.GetIsolate(), "import 'a'; import 'b'; export const c = 'c';",
      "foo.js", kSharableCrossOrigin, TextPosition::MinimumPosition(),
      ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module.IsNull());

  auto requests = module.ModuleRequests(scope.GetScriptState());
  EXPECT_THAT(requests, ::testing::ContainerEq<Vector<String>>({"a", "b"}));
}

TEST(ScriptModuleTest, instantiateNoDeps) {
  V8TestingScope scope;

  auto modulator = new ScriptModuleTestModulator();
  auto resolver = modulator->GetTestScriptModuleResolver();

  Modulator::SetModulator(scope.GetScriptState(), modulator);

  ScriptModule module = ScriptModule::Compile(
      scope.GetIsolate(), "export const a = 42;", "foo.js",
      kSharableCrossOrigin, TextPosition::MinimumPosition(),
      ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module.IsNull());
  ScriptValue exception = module.Instantiate(scope.GetScriptState());
  ASSERT_TRUE(exception.IsEmpty());

  EXPECT_EQ(0u, resolver->ResolveCount());
}

TEST(ScriptModuleTest, instantiateWithDeps) {
  V8TestingScope scope;

  auto modulator = new ScriptModuleTestModulator();
  auto resolver = modulator->GetTestScriptModuleResolver();

  Modulator::SetModulator(scope.GetScriptState(), modulator);

  ScriptModule module_a = ScriptModule::Compile(
      scope.GetIsolate(), "export const a = 'a';", "foo.js",
      kSharableCrossOrigin, TextPosition::MinimumPosition(),
      ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module_a.IsNull());
  resolver->PushScriptModule(module_a);

  ScriptModule module_b = ScriptModule::Compile(
      scope.GetIsolate(), "export const b = 'b';", "foo.js",
      kSharableCrossOrigin, TextPosition::MinimumPosition(),
      ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module_b.IsNull());
  resolver->PushScriptModule(module_b);

  ScriptModule module = ScriptModule::Compile(
      scope.GetIsolate(), "import 'a'; import 'b'; export const c = 123;",
      "c.js", kSharableCrossOrigin, TextPosition::MinimumPosition(),
      ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module.IsNull());
  ScriptValue exception = module.Instantiate(scope.GetScriptState());
  ASSERT_TRUE(exception.IsEmpty());
  EXPECT_EQ(ScriptModuleState::kInstantiated,
            module_a.Status(scope.GetScriptState()));
  EXPECT_EQ(ScriptModuleState::kInstantiated,
            module_b.Status(scope.GetScriptState()));
  EXPECT_EQ(ScriptModuleState::kInstantiated,
            module.Status(scope.GetScriptState()));

  ASSERT_EQ(2u, resolver->ResolveCount());
  EXPECT_EQ("a", resolver->Specifiers()[0]);
  EXPECT_EQ("b", resolver->Specifiers()[1]);
}

TEST(ScriptModuleTest, instantiateError) {
  V8TestingScope scope;

  auto modulator = new ScriptModuleTestModulator();
  auto resolver = modulator->GetTestScriptModuleResolver();

  Modulator::SetModulator(scope.GetScriptState(), modulator);

  ScriptModule module_failure = ScriptModule::Compile(
      scope.GetIsolate(), "nonexistent_function()", "failure.js",
      kSharableCrossOrigin, TextPosition::MinimumPosition(),
      ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module_failure.IsNull());
  module_failure.Instantiate(scope.GetScriptState());
  ASSERT_EQ(ScriptModuleState::kInstantiated,
            module_failure.Status(scope.GetScriptState()));
  module_failure.Evaluate(scope.GetScriptState());
  ASSERT_EQ(ScriptModuleState::kErrored,
            module_failure.Status(scope.GetScriptState()));
  v8::Local<v8::Value> error =
      module_failure.ErrorCompletion(scope.GetScriptState());

  resolver->PushScriptModule(module_failure);

  ScriptModule module = ScriptModule::Compile(
      scope.GetIsolate(), "import 'failure'; export const c = 123;", "c.js",
      kSharableCrossOrigin, TextPosition::MinimumPosition(),
      scope.GetExceptionState());
  ASSERT_FALSE(module.IsNull());
  ScriptValue exception = module.Instantiate(scope.GetScriptState());
  EXPECT_FALSE(exception.IsEmpty());
  ASSERT_EQ(ScriptModuleState::kErrored, module.Status(scope.GetScriptState()));
  v8::Local<v8::Value> error2 = module.ErrorCompletion(scope.GetScriptState());

  EXPECT_EQ(error, error2);
  EXPECT_EQ(error, exception.V8Value());

  ASSERT_EQ(1u, resolver->ResolveCount());
  EXPECT_EQ("failure", resolver->Specifiers()[0]);
}

TEST(ScriptModuleTest, Evaluate) {
  V8TestingScope scope;

  auto modulator = new ScriptModuleTestModulator();
  Modulator::SetModulator(scope.GetScriptState(), modulator);

  ScriptModule module = ScriptModule::Compile(
      scope.GetIsolate(), "export const a = 42; window.foo = 'bar';", "foo.js",
      kSharableCrossOrigin, TextPosition::MinimumPosition(),
      ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module.IsNull());
  ScriptValue exception = module.Instantiate(scope.GetScriptState());
  ASSERT_TRUE(exception.IsEmpty());

  module.Evaluate(scope.GetScriptState());
  v8::Local<v8::Value> value = scope.GetFrame()
                                   .GetScriptController()
                                   .ExecuteScriptInMainWorldAndReturnValue(
                                       ScriptSourceCode("window.foo"));
  ASSERT_TRUE(value->IsString());
  EXPECT_EQ("bar", ToCoreString(v8::Local<v8::String>::Cast(value)));
  EXPECT_EQ(ScriptModuleState::kEvaluated,
            module.Status(scope.GetScriptState()));
}

}  // namespace

}  // namespace blink
