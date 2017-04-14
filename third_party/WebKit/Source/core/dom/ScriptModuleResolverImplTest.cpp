// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ScriptModuleResolverImpl.h"

#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/Modulator.h"
#include "core/dom/ModuleScript.h"
#include "core/testing/DummyModulator.h"
#include "platform/heap/Handle.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/Platform.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class ScriptModuleResolverImplTestModulator final : public DummyModulator {
 public:
  ScriptModuleResolverImplTestModulator() {}
  virtual ~ScriptModuleResolverImplTestModulator() {}

  DECLARE_TRACE();

  int GetFetchedModuleScriptCalled() const {
    return get_fetched_module_script_called_;
  }
  void SetModuleScript(ModuleScript* module_script) {
    module_script_ = module_script;
  }
  const KURL& FetchedUrl() const { return fetched_url_; }

 private:
  // Implements Modulator:
  ModuleScript* GetFetchedModuleScript(const KURL&) override;

  int get_fetched_module_script_called_ = 0;
  KURL fetched_url_;
  Member<ModuleScript> module_script_;
};

DEFINE_TRACE(ScriptModuleResolverImplTestModulator) {
  visitor->Trace(module_script_);
  DummyModulator::Trace(visitor);
}

ModuleScript* ScriptModuleResolverImplTestModulator::GetFetchedModuleScript(
    const KURL& url) {
  get_fetched_module_script_called_++;
  fetched_url_ = url;
  return module_script_.Get();
}

ModuleScript* CreateReferrerModuleScript(Modulator* modulator,
                                         V8TestingScope& scope) {
  ScriptModule referrer_record = ScriptModule::Compile(
      scope.GetIsolate(), "import './target.js'; export const a = 42;",
      "referrer.js", kSharableCrossOrigin);
  KURL referrer_url(kParsedURLString, "https://example.com/referrer.js");
  ModuleScript* referrer_module_script = ModuleScript::Create(
      modulator, referrer_record, referrer_url, "", kParserInserted,
      WebURLRequest::kFetchCredentialsModeOmit);
  // TODO(kouhei): moduleScript->setInstantiateSuccess(); once
  // https://codereview.chromium.org/2782403002/ landed.
  return referrer_module_script;
}

ModuleScript* CreateTargetModuleScript(Modulator* modulator,
                                       V8TestingScope& scope) {
  ScriptModule record =
      ScriptModule::Compile(scope.GetIsolate(), "export const pi = 3.14;",
                            "target.js", kSharableCrossOrigin);
  KURL url(kParsedURLString, "https://example.com/target.js");
  ModuleScript* module_script =
      ModuleScript::Create(modulator, record, url, "", kParserInserted,
                           WebURLRequest::kFetchCredentialsModeOmit);
  // TODO(kouhei): moduleScript->setInstantiateSuccess(); once
  // https://codereview.chromium.org/2782403002/ landed.
  return module_script;
}

}  // namespace

class ScriptModuleResolverImplTest : public testing::Test {
 public:
  void SetUp() override;

  ScriptModuleResolverImplTestModulator* Modulator() {
    return modulator_.Get();
  }

 protected:
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  Persistent<ScriptModuleResolverImplTestModulator> modulator_;
};

void ScriptModuleResolverImplTest::SetUp() {
  platform_->AdvanceClockSeconds(1.);  // For non-zero DocumentParserTimings
  modulator_ = new ScriptModuleResolverImplTestModulator();
}

TEST_F(ScriptModuleResolverImplTest, registerResolveSuccess) {
  ScriptModuleResolverImpl* resolver =
      ScriptModuleResolverImpl::Create(Modulator());
  V8TestingScope scope;

  ModuleScript* referrer_module_script =
      CreateReferrerModuleScript(modulator_, scope);
  resolver->RegisterModuleScript(referrer_module_script);

  ModuleScript* target_module_script =
      CreateTargetModuleScript(modulator_, scope);
  Modulator()->SetModuleScript(target_module_script);

  ScriptModule resolved =
      resolver->Resolve("./target.js", referrer_module_script->Record(),
                        scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(resolved, target_module_script->Record());
  EXPECT_EQ(1, modulator_->GetFetchedModuleScriptCalled());
  EXPECT_EQ(modulator_->FetchedUrl(), target_module_script->BaseURL())
      << "Unexpectedly fetched URL: " << modulator_->FetchedUrl().GetString();
}

TEST_F(ScriptModuleResolverImplTest, resolveInvalidModuleSpecifier) {
  ScriptModuleResolverImpl* resolver =
      ScriptModuleResolverImpl::Create(Modulator());
  V8TestingScope scope;

  ModuleScript* referrer_module_script =
      CreateReferrerModuleScript(modulator_, scope);
  resolver->RegisterModuleScript(referrer_module_script);

  ModuleScript* target_module_script =
      CreateTargetModuleScript(modulator_, scope);
  Modulator()->SetModuleScript(target_module_script);

  ScriptModule resolved = resolver->Resolve(
      "invalid", referrer_module_script->Record(), scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(kV8TypeError, scope.GetExceptionState().Code());
  EXPECT_TRUE(resolved.IsNull());
  EXPECT_EQ(0, modulator_->GetFetchedModuleScriptCalled());
}

TEST_F(ScriptModuleResolverImplTest, resolveLoadFailedModule) {
  ScriptModuleResolverImpl* resolver =
      ScriptModuleResolverImpl::Create(Modulator());
  V8TestingScope scope;

  ModuleScript* referrer_module_script =
      CreateReferrerModuleScript(modulator_, scope);
  resolver->RegisterModuleScript(referrer_module_script);

  ModuleScript* target_module_script =
      CreateTargetModuleScript(modulator_, scope);
  // Set Modulator::getFetchedModuleScript to return nullptr, which represents
  // that the target module failed to load.
  Modulator()->SetModuleScript(nullptr);

  ScriptModule resolved =
      resolver->Resolve("./target.js", referrer_module_script->Record(),
                        scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(kV8TypeError, scope.GetExceptionState().Code());
  EXPECT_TRUE(resolved.IsNull());
  EXPECT_EQ(1, modulator_->GetFetchedModuleScriptCalled());
  EXPECT_EQ(modulator_->FetchedUrl(), target_module_script->BaseURL())
      << "Unexpectedly fetched URL: " << modulator_->FetchedUrl().GetString();
}

}  // namespace blink
