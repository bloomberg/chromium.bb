// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DynamicModuleResolver.h"

#include "bindings/core/v8/ReferrerScriptInfo.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/Document.h"
#include "core/dom/ModuleScript.h"
#include "core/loader/modulescript/ModuleScriptFetchRequest.h"
#include "core/testing/DummyModulator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

constexpr const char* kTestReferrerURL = "https://example.com/referrer.js";
constexpr const char* kTestDependencyURL = "https://example.com/dependency.js";

const KURL TestReferrerURL() {
  return KURL(kTestReferrerURL);
}
const KURL TestDependencyURL() {
  return KURL(kTestDependencyURL);
}

class DynamicModuleResolverTestModulator final : public DummyModulator {
 public:
  explicit DynamicModuleResolverTestModulator(ScriptState* script_state)
      : script_state_(script_state) {}
  ~DynamicModuleResolverTestModulator() override = default;

  void ResolveTreeFetch(ModuleScript* module_script) {
    ASSERT_TRUE(pending_client_);
    pending_client_->NotifyModuleTreeLoadFinished(module_script);
    pending_client_ = nullptr;
  }

  void Trace(blink::Visitor*);

 private:
  // Implements Modulator:
  ReferrerPolicy GetReferrerPolicy() override { return kReferrerPolicyDefault; }
  ScriptState* GetScriptState() final { return script_state_.get(); }

  ModuleScript* GetFetchedModuleScript(const KURL& url) final {
    EXPECT_EQ(TestReferrerURL(), url);
    ModuleScript* module_script =
        ModuleScript::CreateForTest(this, ScriptModule(), url);
    return module_script;
  }

  void FetchTree(const ModuleScriptFetchRequest& request,
                 ModuleTreeClient* client) final {
    EXPECT_EQ(TestDependencyURL(), request.Url());

    pending_client_ = client;
  }

  ScriptValue ExecuteModule(const ModuleScript* module_script,
                            CaptureEvalErrorFlag capture_error) final {
    EXPECT_EQ(CaptureEvalErrorFlag::kCapture, capture_error);

    ScriptState::Scope scope(script_state_.get());
    return module_script->Record().Evaluate(script_state_.get(),
                                            CaptureEvalErrorFlag::kCapture);
  }

  ScriptModuleState GetRecordStatus(ScriptModule script_module) final {
    ScriptState::Scope scope(script_state_.get());
    return script_module.Status(script_state_.get());
  }

  ScriptValue GetError(const ModuleScript* module_script) final {
    ScriptState::Scope scope(script_state_.get());
    ScriptModule record = module_script->Record();
    DCHECK(!record.IsNull());
    return ScriptValue(script_state_.get(),
                       record.ErrorCompletion(script_state_.get()));
  }

  scoped_refptr<ScriptState> script_state_;
  Member<ModuleTreeClient> pending_client_;
};

void DynamicModuleResolverTestModulator::Trace(blink::Visitor* visitor) {
  visitor->Trace(pending_client_);
  DummyModulator::Trace(visitor);
}

// CaptureExportedStringFunction implements a javascript function
// with a single argument of type module namespace.
// CaptureExportedStringFunction captures the exported string value
// from the module namespace as a blink::String, exposed via CapturedValue().
class CaptureExportedStringFunction final : public ScriptFunction {
 public:
  CaptureExportedStringFunction(ScriptState* script_state,
                                const String& export_name)
      : ScriptFunction(script_state), export_name_(export_name) {}

  v8::Local<v8::Function> Bind() { return BindToV8Function(); }
  bool WasCalled() const { return was_called_; }
  const String& CapturedValue() const { return captured_value_; }

 private:
  ScriptValue Call(ScriptValue value) override {
    was_called_ = true;

    v8::Isolate* isolate = GetScriptState()->GetIsolate();
    v8::Local<v8::Context> context = GetScriptState()->GetContext();

    v8::Local<v8::Object> module_namespace =
        value.V8Value()->ToObject(context).ToLocalChecked();
    v8::Local<v8::Value> exported_value =
        module_namespace->Get(context, V8String(isolate, export_name_))
            .ToLocalChecked();
    captured_value_ = ToCoreString(exported_value->ToString());

    return ScriptValue();
  }

  const String export_name_;
  bool was_called_ = false;
  String captured_value_;
};

// CaptureErrorFunction implements a javascript function which captures
// name and error of the exception passed as its argument.
class CaptureErrorFunction final : public ScriptFunction {
 public:
  explicit CaptureErrorFunction(ScriptState* script_state)
      : ScriptFunction(script_state) {}

  v8::Local<v8::Function> Bind() { return BindToV8Function(); }
  bool WasCalled() const { return was_called_; }
  const String& Name() const { return name_; }
  const String& Message() const { return message_; }

 private:
  ScriptValue Call(ScriptValue value) override {
    was_called_ = true;

    v8::Isolate* isolate = GetScriptState()->GetIsolate();
    v8::Local<v8::Context> context = GetScriptState()->GetContext();

    v8::Local<v8::Object> error_object =
        value.V8Value()->ToObject(context).ToLocalChecked();

    v8::Local<v8::Value> name =
        error_object->Get(context, V8String(isolate, "name")).ToLocalChecked();
    name_ = ToCoreString(name->ToString());
    v8::Local<v8::Value> message =
        error_object->Get(context, V8String(isolate, "message"))
            .ToLocalChecked();
    message_ = ToCoreString(message->ToString());

    return ScriptValue();
  }

  bool was_called_ = false;
  String name_;
  String message_;
};

class NotReached final : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state) {
    auto not_reached = new NotReached(script_state);
    return not_reached->BindToV8Function();
  }

 private:
  explicit NotReached(ScriptState* script_state)
      : ScriptFunction(script_state) {}

  ScriptValue Call(ScriptValue) override {
    ADD_FAILURE();
    return ScriptValue();
  }
};

}  // namespace

TEST(DynamicModuleResolverTest, ResolveSuccess) {
  V8TestingScope scope;
  DynamicModuleResolverTestModulator* modulator =
      new DynamicModuleResolverTestModulator(scope.GetScriptState());

  auto promise_resolver = ScriptPromiseResolver::Create(scope.GetScriptState());
  ScriptPromise promise = promise_resolver->Promise();

  auto capture =
      new CaptureExportedStringFunction(scope.GetScriptState(), "foo");
  promise.Then(capture->Bind(),
               NotReached::CreateFunction(scope.GetScriptState()));

  auto resolver = DynamicModuleResolver::Create(modulator);
  resolver->ResolveDynamically("./dependency.js", TestReferrerURL(),
                               ReferrerScriptInfo(), promise_resolver);

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_FALSE(capture->WasCalled());

  ScriptModule record = ScriptModule::Compile(
      scope.GetIsolate(), "export const foo = 'hello';", TestReferrerURL(),
      kSharableCrossOrigin, network::mojom::FetchCredentialsMode::kOmit, "",
      kParserInserted, TextPosition::MinimumPosition(), ASSERT_NO_EXCEPTION);
  ModuleScript* module_script =
      ModuleScript::CreateForTest(modulator, record, TestDependencyURL());
  EXPECT_TRUE(record.Instantiate(scope.GetScriptState()).IsEmpty());
  modulator->ResolveTreeFetch(module_script);

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(capture->WasCalled());
  EXPECT_EQ("hello", capture->CapturedValue());
}

TEST(DynamicModuleResolverTest, ResolveSpecifierFailure) {
  V8TestingScope scope;
  auto modulator =
      new DynamicModuleResolverTestModulator(scope.GetScriptState());

  auto promise_resolver = ScriptPromiseResolver::Create(scope.GetScriptState());
  ScriptPromise promise = promise_resolver->Promise();

  auto capture = new CaptureErrorFunction(scope.GetScriptState());
  promise.Then(NotReached::CreateFunction(scope.GetScriptState()),
               capture->Bind());

  auto resolver = DynamicModuleResolver::Create(modulator);
  resolver->ResolveDynamically("invalid-specifier", TestReferrerURL(),
                               ReferrerScriptInfo(), promise_resolver);

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(capture->WasCalled());
  EXPECT_EQ("TypeError", capture->Name());
  EXPECT_TRUE(capture->Message().StartsWith("Failed to resolve"));
}

TEST(DynamicModuleResolverTest, FetchFailure) {
  V8TestingScope scope;
  auto modulator =
      new DynamicModuleResolverTestModulator(scope.GetScriptState());

  auto promise_resolver = ScriptPromiseResolver::Create(scope.GetScriptState());
  ScriptPromise promise = promise_resolver->Promise();

  auto capture = new CaptureErrorFunction(scope.GetScriptState());
  promise.Then(NotReached::CreateFunction(scope.GetScriptState()),
               capture->Bind());

  auto resolver = DynamicModuleResolver::Create(modulator);
  resolver->ResolveDynamically("./dependency.js", TestReferrerURL(),
                               ReferrerScriptInfo(), promise_resolver);

  EXPECT_FALSE(capture->WasCalled());

  modulator->ResolveTreeFetch(nullptr);

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(capture->WasCalled());
  EXPECT_EQ("TypeError", capture->Name());
  EXPECT_TRUE(capture->Message().StartsWith("Failed to fetch"));
}

TEST(DynamicModuleResolverTest, ExceptionThrown) {
  V8TestingScope scope;
  auto modulator =
      new DynamicModuleResolverTestModulator(scope.GetScriptState());

  auto promise_resolver = ScriptPromiseResolver::Create(scope.GetScriptState());
  ScriptPromise promise = promise_resolver->Promise();

  auto capture = new CaptureErrorFunction(scope.GetScriptState());
  promise.Then(NotReached::CreateFunction(scope.GetScriptState()),
               capture->Bind());

  auto resolver = DynamicModuleResolver::Create(modulator);
  resolver->ResolveDynamically("./dependency.js", TestReferrerURL(),
                               ReferrerScriptInfo(), promise_resolver);

  EXPECT_FALSE(capture->WasCalled());

  ScriptModule record = ScriptModule::Compile(
      scope.GetIsolate(), "throw Error('bar')", TestReferrerURL(),
      kSharableCrossOrigin, network::mojom::FetchCredentialsMode::kOmit, "",
      kParserInserted, TextPosition::MinimumPosition(), ASSERT_NO_EXCEPTION);
  ModuleScript* module_script =
      ModuleScript::CreateForTest(modulator, record, TestDependencyURL());
  EXPECT_TRUE(record.Instantiate(scope.GetScriptState()).IsEmpty());
  modulator->ResolveTreeFetch(module_script);

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(capture->WasCalled());
  EXPECT_EQ("Error", capture->Name());
  EXPECT_EQ("bar", capture->Message());
}

TEST(DynamicModuleResolverTest, ResolveWithNullReferrerScriptSuccess) {
  V8TestingScope scope;
  scope.GetDocument().SetURL(KURL("https://example.com"));

  auto modulator =
      new DynamicModuleResolverTestModulator(scope.GetScriptState());

  auto promise_resolver = ScriptPromiseResolver::Create(scope.GetScriptState());
  ScriptPromise promise = promise_resolver->Promise();

  auto capture =
      new CaptureExportedStringFunction(scope.GetScriptState(), "foo");
  promise.Then(capture->Bind(),
               NotReached::CreateFunction(scope.GetScriptState()));

  auto resolver = DynamicModuleResolver::Create(modulator);
  resolver->ResolveDynamically("./dependency.js", /* null referrer */ KURL(),
                               ReferrerScriptInfo(), promise_resolver);

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_FALSE(capture->WasCalled());

  ScriptModule record = ScriptModule::Compile(
      scope.GetIsolate(), "export const foo = 'hello';", TestDependencyURL(),
      kSharableCrossOrigin, network::mojom::FetchCredentialsMode::kOmit, "",
      kParserInserted, TextPosition::MinimumPosition(), ASSERT_NO_EXCEPTION);
  ModuleScript* module_script =
      ModuleScript::CreateForTest(modulator, record, TestDependencyURL());
  EXPECT_TRUE(record.Instantiate(scope.GetScriptState()).IsEmpty());
  modulator->ResolveTreeFetch(module_script);

  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(capture->WasCalled());
  EXPECT_EQ("hello", capture->CapturedValue());
}

}  // namespace blink
