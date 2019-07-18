// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/module_record.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/script/module_record_resolver.h"
#include "third_party/blink/renderer/core/testing/dummy_modulator.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class TestModuleRecordResolver final : public ModuleRecordResolver {
 public:
  TestModuleRecordResolver() = default;
  ~TestModuleRecordResolver() override = default;

  size_t ResolveCount() const { return specifiers_.size(); }
  const Vector<String>& Specifiers() const { return specifiers_; }
  void PushModuleRecord(ModuleRecord module_record) {
    module_records_.push_back(module_record);
  }

 private:
  // Implements ModuleRecordResolver:

  void RegisterModuleScript(const ModuleScript*) override { NOTREACHED(); }
  void UnregisterModuleScript(const ModuleScript*) override { NOTREACHED(); }
  const ModuleScript* GetModuleScriptFromModuleRecord(
      const ModuleRecord&) const override {
    NOTREACHED();
    return nullptr;
  }

  ModuleRecord Resolve(const String& specifier,
                       const ModuleRecord&,
                       ExceptionState&) override {
    specifiers_.push_back(specifier);
    return module_records_.TakeFirst();
  }

  Vector<String> specifiers_;
  Deque<ModuleRecord> module_records_;
};

class ModuleRecordTestModulator final : public DummyModulator {
 public:
  ModuleRecordTestModulator();
  ~ModuleRecordTestModulator() override = default;

  void Trace(blink::Visitor*) override;

  TestModuleRecordResolver* GetTestModuleRecordResolver() {
    return resolver_.Get();
  }

 private:
  // Implements Modulator:

  ModuleRecordResolver* GetModuleRecordResolver() override {
    return resolver_.Get();
  }

  Member<TestModuleRecordResolver> resolver_;
};

ModuleRecordTestModulator::ModuleRecordTestModulator()
    : resolver_(MakeGarbageCollected<TestModuleRecordResolver>()) {}

void ModuleRecordTestModulator::Trace(blink::Visitor* visitor) {
  visitor->Trace(resolver_);
  DummyModulator::Trace(visitor);
}

TEST(ModuleRecordTest, compileSuccess) {
  V8TestingScope scope;
  const KURL js_url("https://example.com/foo.js");
  ModuleRecord module = ModuleRecord::Compile(
      scope.GetIsolate(), "export const a = 42;", js_url, js_url,
      ScriptFetchOptions(), TextPosition::MinimumPosition(),
      ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module.IsNull());
}

TEST(ModuleRecordTest, compileFail) {
  V8TestingScope scope;
  const KURL js_url("https://example.com/foo.js");
  ModuleRecord module = ModuleRecord::Compile(
      scope.GetIsolate(), "123 = 456", js_url, js_url, ScriptFetchOptions(),
      TextPosition::MinimumPosition(), scope.GetExceptionState());
  ASSERT_TRUE(module.IsNull());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

TEST(ModuleRecordTest, equalAndHash) {
  V8TestingScope scope;
  const KURL js_url_a("https://example.com/a.js");
  const KURL js_url_b("https://example.com/b.js");

  ModuleRecord module_null;
  ModuleRecord module_a = ModuleRecord::Compile(
      scope.GetIsolate(), "export const a = 'a';", js_url_a, js_url_a,
      ScriptFetchOptions(), TextPosition::MinimumPosition(),
      ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module_a.IsNull());
  ModuleRecord module_b = ModuleRecord::Compile(
      scope.GetIsolate(), "export const b = 'b';", js_url_b, js_url_b,
      ScriptFetchOptions(), TextPosition::MinimumPosition(),
      ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module_b.IsNull());
  Vector<char> module_deleted_buffer(sizeof(ModuleRecord));
  ModuleRecord& module_deleted =
      *reinterpret_cast<ModuleRecord*>(module_deleted_buffer.data());
  HashTraits<ModuleRecord>::ConstructDeletedValue(module_deleted, true);

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

  EXPECT_NE(DefaultHash<ModuleRecord>::Hash::GetHash(module_a),
            DefaultHash<ModuleRecord>::Hash::GetHash(module_b));
  EXPECT_NE(DefaultHash<ModuleRecord>::Hash::GetHash(module_null),
            DefaultHash<ModuleRecord>::Hash::GetHash(module_a));
  EXPECT_NE(DefaultHash<ModuleRecord>::Hash::GetHash(module_null),
            DefaultHash<ModuleRecord>::Hash::GetHash(module_b));
}

TEST(ModuleRecordTest, moduleRequests) {
  V8TestingScope scope;
  const KURL js_url("https://example.com/foo.js");
  ModuleRecord module = ModuleRecord::Compile(
      scope.GetIsolate(), "import 'a'; import 'b'; export const c = 'c';",
      js_url, js_url, ScriptFetchOptions(), TextPosition::MinimumPosition(),
      ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module.IsNull());

  auto requests = module.ModuleRequests(scope.GetScriptState());
  EXPECT_THAT(requests, testing::ContainerEq<Vector<String>>({"a", "b"}));
}

TEST(ModuleRecordTest, instantiateNoDeps) {
  V8TestingScope scope;

  auto* modulator = MakeGarbageCollected<ModuleRecordTestModulator>();
  auto* resolver = modulator->GetTestModuleRecordResolver();

  Modulator::SetModulator(scope.GetScriptState(), modulator);

  const KURL js_url("https://example.com/foo.js");
  ModuleRecord module = ModuleRecord::Compile(
      scope.GetIsolate(), "export const a = 42;", js_url, js_url,
      ScriptFetchOptions(), TextPosition::MinimumPosition(),
      ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module.IsNull());
  ScriptValue exception = module.Instantiate(scope.GetScriptState());
  ASSERT_TRUE(exception.IsEmpty());

  EXPECT_EQ(0u, resolver->ResolveCount());
}

TEST(ModuleRecordTest, instantiateWithDeps) {
  V8TestingScope scope;

  auto* modulator = MakeGarbageCollected<ModuleRecordTestModulator>();
  auto* resolver = modulator->GetTestModuleRecordResolver();

  Modulator::SetModulator(scope.GetScriptState(), modulator);

  const KURL js_url_a("https://example.com/a.js");
  ModuleRecord module_a = ModuleRecord::Compile(
      scope.GetIsolate(), "export const a = 'a';", js_url_a, js_url_a,
      ScriptFetchOptions(), TextPosition::MinimumPosition(),
      ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module_a.IsNull());
  resolver->PushModuleRecord(module_a);

  const KURL js_url_b("https://example.com/b.js");
  ModuleRecord module_b = ModuleRecord::Compile(
      scope.GetIsolate(), "export const b = 'b';", js_url_b, js_url_b,
      ScriptFetchOptions(), TextPosition::MinimumPosition(),
      ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module_b.IsNull());
  resolver->PushModuleRecord(module_b);

  const KURL js_url_c("https://example.com/c.js");
  ModuleRecord module = ModuleRecord::Compile(
      scope.GetIsolate(), "import 'a'; import 'b'; export const c = 123;",
      js_url_c, js_url_c, ScriptFetchOptions(), TextPosition::MinimumPosition(),
      ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module.IsNull());
  ScriptValue exception = module.Instantiate(scope.GetScriptState());
  ASSERT_TRUE(exception.IsEmpty());

  ASSERT_EQ(2u, resolver->ResolveCount());
  EXPECT_EQ("a", resolver->Specifiers()[0]);
  EXPECT_EQ("b", resolver->Specifiers()[1]);
}

TEST(ModuleRecordTest, EvaluationErrrorIsRemembered) {
  V8TestingScope scope;

  auto* modulator = MakeGarbageCollected<ModuleRecordTestModulator>();
  auto* resolver = modulator->GetTestModuleRecordResolver();

  Modulator::SetModulator(scope.GetScriptState(), modulator);

  const KURL js_url_f("https://example.com/failure.js");
  ModuleRecord module_failure = ModuleRecord::Compile(
      scope.GetIsolate(), "nonexistent_function()", js_url_f, js_url_f,
      ScriptFetchOptions(), TextPosition::MinimumPosition(),
      ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module_failure.IsNull());
  ASSERT_TRUE(module_failure.Instantiate(scope.GetScriptState()).IsEmpty());
  ScriptValue evaluation_error =
      module_failure.Evaluate(scope.GetScriptState());
  EXPECT_FALSE(evaluation_error.IsEmpty());

  resolver->PushModuleRecord(module_failure);

  const KURL js_url_c("https://example.com/c.js");
  ModuleRecord module = ModuleRecord::Compile(
      scope.GetIsolate(), "import 'failure'; export const c = 123;", js_url_c,
      js_url_c, ScriptFetchOptions(), TextPosition::MinimumPosition(),
      scope.GetExceptionState());
  ASSERT_FALSE(module.IsNull());
  ASSERT_TRUE(module.Instantiate(scope.GetScriptState()).IsEmpty());
  ScriptValue evaluation_error2 = module.Evaluate(scope.GetScriptState());
  EXPECT_FALSE(evaluation_error2.IsEmpty());

  EXPECT_EQ(evaluation_error, evaluation_error2);

  ASSERT_EQ(1u, resolver->ResolveCount());
  EXPECT_EQ("failure", resolver->Specifiers()[0]);
}

TEST(ModuleRecordTest, Evaluate) {
  V8TestingScope scope;

  auto* modulator = MakeGarbageCollected<ModuleRecordTestModulator>();
  Modulator::SetModulator(scope.GetScriptState(), modulator);

  const KURL js_url("https://example.com/foo.js");
  ModuleRecord module = ModuleRecord::Compile(
      scope.GetIsolate(), "export const a = 42; window.foo = 'bar';", js_url,
      js_url, ScriptFetchOptions(), TextPosition::MinimumPosition(),
      ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module.IsNull());
  ScriptValue exception = module.Instantiate(scope.GetScriptState());
  ASSERT_TRUE(exception.IsEmpty());

  EXPECT_TRUE(module.Evaluate(scope.GetScriptState()).IsEmpty());
  v8::Local<v8::Value> value = scope.GetFrame()
                                   .GetScriptController()
                                   .ExecuteScriptInMainWorldAndReturnValue(
                                       ScriptSourceCode("window.foo"), KURL(),
                                       SanitizeScriptErrors::kSanitize);
  ASSERT_TRUE(value->IsString());
  EXPECT_EQ("bar", ToCoreString(v8::Local<v8::String>::Cast(value)));

  v8::Local<v8::Object> module_namespace =
      v8::Local<v8::Object>::Cast(module.V8Namespace(scope.GetIsolate()));
  EXPECT_FALSE(module_namespace.IsEmpty());
  v8::Local<v8::Value> exported_value =
      module_namespace
          ->Get(scope.GetContext(), V8String(scope.GetIsolate(), "a"))
          .ToLocalChecked();
  EXPECT_EQ(42.0, exported_value->NumberValue(scope.GetContext()).ToChecked());
}

TEST(ModuleRecordTest, EvaluateCaptureError) {
  V8TestingScope scope;

  auto* modulator = MakeGarbageCollected<ModuleRecordTestModulator>();
  Modulator::SetModulator(scope.GetScriptState(), modulator);

  const KURL js_url("https://example.com/foo.js");
  ModuleRecord module = ModuleRecord::Compile(
      scope.GetIsolate(), "throw 'bar';", js_url, js_url, ScriptFetchOptions(),
      TextPosition::MinimumPosition(), ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(module.IsNull());
  ScriptValue exception = module.Instantiate(scope.GetScriptState());
  ASSERT_TRUE(exception.IsEmpty());

  ScriptValue error = module.Evaluate(scope.GetScriptState());
  ASSERT_FALSE(error.IsEmpty());
  ASSERT_TRUE(error.V8Value()->IsString());
  EXPECT_EQ("bar", ToCoreString(v8::Local<v8::String>::Cast(error.V8Value())));
}

}  // namespace

}  // namespace blink
