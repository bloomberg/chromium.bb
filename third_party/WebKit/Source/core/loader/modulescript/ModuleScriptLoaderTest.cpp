// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/modulescript/ModuleScriptLoader.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/Document.h"
#include "core/dom/Modulator.h"
#include "core/dom/ModuleScript.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/loader/modulescript/DocumentModuleScriptFetcher.h"
#include "core/loader/modulescript/ModuleScriptFetchRequest.h"
#include "core/loader/modulescript/ModuleScriptLoaderClient.h"
#include "core/loader/modulescript/ModuleScriptLoaderRegistry.h"
#include "core/loader/modulescript/WorkletModuleScriptFetcher.h"
#include "core/testing/DummyModulator.h"
#include "core/testing/DummyPageHolder.h"
#include "core/workers/MainThreadWorkletGlobalScope.h"
#include "core/workers/MainThreadWorkletReportingProxy.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/testing/FetchTestingPlatformSupport.h"
#include "platform/loader/testing/MockFetchContext.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class TestModuleScriptLoaderClient final
    : public GarbageCollectedFinalized<TestModuleScriptLoaderClient>,
      public ModuleScriptLoaderClient {
  USING_GARBAGE_COLLECTED_MIXIN(TestModuleScriptLoaderClient);

 public:
  TestModuleScriptLoaderClient() = default;
  ~TestModuleScriptLoaderClient() override {}

  void Trace(blink::Visitor* visitor) { visitor->Trace(module_script_); }

  void NotifyNewSingleModuleFinished(ModuleScript* module_script) override {
    was_notify_finished_ = true;
    module_script_ = module_script;
  }

  bool WasNotifyFinished() const { return was_notify_finished_; }
  ModuleScript* GetModuleScript() { return module_script_; }

 private:
  bool was_notify_finished_ = false;
  Member<ModuleScript> module_script_;
};

class ModuleScriptLoaderTestModulator final : public DummyModulator {
 public:
  ModuleScriptLoaderTestModulator(RefPtr<ScriptState> script_state,
                                  RefPtr<SecurityOrigin> security_origin)
      : script_state_(std::move(script_state)),
        security_origin_(std::move(security_origin)) {
    auto* fetch_context =
        MockFetchContext::Create(MockFetchContext::kShouldLoadNewResource);
    fetcher_ = ResourceFetcher::Create(fetch_context);
  }

  ~ModuleScriptLoaderTestModulator() override {}

  SecurityOrigin* GetSecurityOriginForFetch() override {
    return security_origin_.get();
  }

  ScriptState* GetScriptState() override { return script_state_.get(); }

  ScriptModule CompileModule(
      const String& script,
      const String& url_str,
      AccessControlStatus access_control_status,
      WebURLRequest::FetchCredentialsMode credentials_mode,
      const String& nonce,
      ParserDisposition parser_state,
      const TextPosition& position,
      ExceptionState& exception_state) override {
    ScriptState::Scope scope(script_state_.get());
    return ScriptModule::Compile(script_state_->GetIsolate(), script, url_str,
                                 access_control_status, credentials_mode, nonce,
                                 parser_state, position, exception_state);
  }

  void SetModuleRequests(const Vector<String>& requests) {
    requests_.clear();
    for (const String& request : requests) {
      requests_.emplace_back(request, TextPosition::MinimumPosition());
    }
  }
  Vector<ModuleRequest> ModuleRequestsFromScriptModule(ScriptModule) override {
    return requests_;
  }
  ScriptModuleState GetRecordStatus(ScriptModule) override {
    return ScriptModuleState::kUninstantiated;
  }

  ModuleScriptFetcher* CreateModuleScriptFetcher() override {
    auto* execution_context = ExecutionContext::From(script_state_.get());
    if (execution_context->IsDocument())
      return new DocumentModuleScriptFetcher(Fetcher());
    auto* global_scope = ToWorkletGlobalScope(execution_context);
    return new WorkletModuleScriptFetcher(
        global_scope->ModuleResponsesMapProxy());
  }

  ResourceFetcher* Fetcher() const { return fetcher_.Get(); }

  void Trace(blink::Visitor*);

 private:
  RefPtr<ScriptState> script_state_;
  RefPtr<SecurityOrigin> security_origin_;
  Member<ResourceFetcher> fetcher_;
  Vector<ModuleRequest> requests_;
};

void ModuleScriptLoaderTestModulator::Trace(blink::Visitor* visitor) {
  visitor->Trace(fetcher_);
  DummyModulator::Trace(visitor);
}

}  // namespace

class ModuleScriptLoaderTest : public ::testing::Test {
  DISALLOW_COPY_AND_ASSIGN(ModuleScriptLoaderTest);

 public:
  ModuleScriptLoaderTest() = default;
  void SetUp() override;

  void InitializeForDocument();
  void InitializeForWorklet();

  void TestFetchDataURL(TestModuleScriptLoaderClient*);
  void TestInvalidSpecifier(TestModuleScriptLoaderClient*);
  void TestFetchInvalidURL(TestModuleScriptLoaderClient*);
  void TestFetchURL(TestModuleScriptLoaderClient*);

  LocalFrame& GetFrame() { return dummy_page_holder_->GetFrame(); }
  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }
  ModuleScriptLoaderTestModulator* GetModulator() { return modulator_.Get(); }

 protected:
  ScopedTestingPlatformSupport<FetchTestingPlatformSupport> platform_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  std::unique_ptr<MainThreadWorkletReportingProxy> reporting_proxy_;
  Persistent<ModuleScriptLoaderTestModulator> modulator_;
  Persistent<MainThreadWorkletGlobalScope> global_scope_;
};

void ModuleScriptLoaderTest::SetUp() {
  platform_->AdvanceClockSeconds(1.);  // For non-zero DocumentParserTimings
  dummy_page_holder_ = DummyPageHolder::Create(IntSize(500, 500));
  GetDocument().SetURL(KURL(kParsedURLString, "https://example.test"));
  GetDocument().SetSecurityOrigin(SecurityOrigin::Create(GetDocument().Url()));
}

void ModuleScriptLoaderTest::InitializeForDocument() {
  modulator_ = new ModuleScriptLoaderTestModulator(
      ToScriptStateForMainWorld(&GetFrame()),
      GetDocument().GetSecurityOrigin());
}

void ModuleScriptLoaderTest::InitializeForWorklet() {
  reporting_proxy_ =
      WTF::MakeUnique<MainThreadWorkletReportingProxy>(&GetDocument());
  global_scope_ = new MainThreadWorkletGlobalScope(
      &GetFrame(), KURL(kParsedURLString, "https://example.test/worklet.js"),
      "fake user agent", ToIsolate(&GetDocument()), *reporting_proxy_);
  global_scope_->ScriptController()->InitializeContextIfNeeded("Dummy Context");
  modulator_ = new ModuleScriptLoaderTestModulator(
      global_scope_->ScriptController()->GetScriptState(),
      GetDocument().GetSecurityOrigin());
  global_scope_->SetModuleResponsesMapProxyForTesting(
      WorkletModuleResponsesMapProxy::Create(
          new WorkletModuleResponsesMap(modulator_->Fetcher()),
          TaskRunnerHelper::Get(TaskType::kUnspecedLoading, &GetDocument()),
          TaskRunnerHelper::Get(TaskType::kUnspecedLoading, global_scope_)));
}

void ModuleScriptLoaderTest::TestFetchDataURL(
    TestModuleScriptLoaderClient* client) {
  ModuleScriptLoaderRegistry* registry = ModuleScriptLoaderRegistry::Create();
  KURL url(kParsedURLString, "data:text/javascript,export default 'grapes';");
  ModuleScriptFetchRequest module_request(url, ScriptFetchOptions());
  registry->Fetch(module_request, ModuleGraphLevel::kTopLevelModuleFetch,
                  GetModulator(), client);
}

TEST_F(ModuleScriptLoaderTest, FetchDataURL) {
  InitializeForDocument();
  TestModuleScriptLoaderClient* client = new TestModuleScriptLoaderClient;
  TestFetchDataURL(client);

  EXPECT_TRUE(client->WasNotifyFinished())
      << "ModuleScriptLoader should finish synchronously.";
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_FALSE(client->GetModuleScript()->HasEmptyRecord());
}

TEST_F(ModuleScriptLoaderTest, FetchDataURL_OnWorklet) {
  InitializeForWorklet();
  TestModuleScriptLoaderClient* client1 = new TestModuleScriptLoaderClient;
  TestFetchDataURL(client1);

  EXPECT_FALSE(client1->WasNotifyFinished())
      << "ModuleScriptLoader should finish asynchronously.";
  platform_->RunUntilIdle();

  EXPECT_TRUE(client1->WasNotifyFinished());
  ASSERT_TRUE(client1->GetModuleScript());
  EXPECT_FALSE(client1->GetModuleScript()->HasEmptyRecord());

  // Try to fetch the same URL again in order to verify the case where
  // WorkletModuleResponsesMap serves a cache.
  TestModuleScriptLoaderClient* client2 = new TestModuleScriptLoaderClient;
  TestFetchDataURL(client2);

  EXPECT_FALSE(client2->WasNotifyFinished())
      << "ModuleScriptLoader should finish asynchronously.";
  platform_->RunUntilIdle();

  EXPECT_TRUE(client2->WasNotifyFinished());
  ASSERT_TRUE(client2->GetModuleScript());
  EXPECT_FALSE(client2->GetModuleScript()->HasEmptyRecord());
}

void ModuleScriptLoaderTest::TestInvalidSpecifier(
    TestModuleScriptLoaderClient* client) {
  ModuleScriptLoaderRegistry* registry = ModuleScriptLoaderRegistry::Create();
  KURL url(kParsedURLString,
           "data:text/javascript,import 'invalid';export default 'grapes';");
  ModuleScriptFetchRequest module_request(url, ScriptFetchOptions());
  GetModulator()->SetModuleRequests({"invalid"});
  registry->Fetch(module_request, ModuleGraphLevel::kTopLevelModuleFetch,
                  GetModulator(), client);
}

TEST_F(ModuleScriptLoaderTest, InvalidSpecifier) {
  InitializeForDocument();
  TestModuleScriptLoaderClient* client = new TestModuleScriptLoaderClient;
  TestInvalidSpecifier(client);

  EXPECT_TRUE(client->WasNotifyFinished())
      << "ModuleScriptLoader should finish synchronously.";
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_TRUE(client->GetModuleScript()->HasEmptyRecord());
}

TEST_F(ModuleScriptLoaderTest, InvalidSpecifier_OnWorklet) {
  InitializeForWorklet();
  TestModuleScriptLoaderClient* client = new TestModuleScriptLoaderClient;
  TestInvalidSpecifier(client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleScriptLoader should finish asynchronously.";
  platform_->RunUntilIdle();

  EXPECT_TRUE(client->WasNotifyFinished());
  ASSERT_TRUE(client->GetModuleScript());
  EXPECT_TRUE(client->GetModuleScript()->HasEmptyRecord());
}

void ModuleScriptLoaderTest::TestFetchInvalidURL(
    TestModuleScriptLoaderClient* client) {
  ModuleScriptLoaderRegistry* registry = ModuleScriptLoaderRegistry::Create();
  KURL url;
  EXPECT_FALSE(url.IsValid());
  ModuleScriptFetchRequest module_request(url, ScriptFetchOptions());
  registry->Fetch(module_request, ModuleGraphLevel::kTopLevelModuleFetch,
                  GetModulator(), client);
}

TEST_F(ModuleScriptLoaderTest, FetchInvalidURL) {
  InitializeForDocument();
  TestModuleScriptLoaderClient* client = new TestModuleScriptLoaderClient;
  TestFetchInvalidURL(client);

  EXPECT_TRUE(client->WasNotifyFinished())
      << "ModuleScriptLoader should finish synchronously.";
  EXPECT_FALSE(client->GetModuleScript());
}

TEST_F(ModuleScriptLoaderTest, FetchInvalidURL_OnWorklet) {
  InitializeForWorklet();
  TestModuleScriptLoaderClient* client = new TestModuleScriptLoaderClient;
  TestFetchInvalidURL(client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleScriptLoader should finish asynchronously.";
  platform_->RunUntilIdle();

  EXPECT_TRUE(client->WasNotifyFinished());
  EXPECT_FALSE(client->GetModuleScript());
}

void ModuleScriptLoaderTest::TestFetchURL(
    TestModuleScriptLoaderClient* client) {
  KURL url(kParsedURLString, "https://example.test/module.js");
  URLTestHelpers::RegisterMockedURLLoad(
      url, testing::CoreTestDataPath("module.js"), "text/javascript");

  ModuleScriptLoaderRegistry* registry = ModuleScriptLoaderRegistry::Create();
  ModuleScriptFetchRequest module_request(url, ScriptFetchOptions());
  registry->Fetch(module_request, ModuleGraphLevel::kTopLevelModuleFetch,
                  GetModulator(), client);
}

TEST_F(ModuleScriptLoaderTest, FetchURL) {
  InitializeForDocument();
  TestModuleScriptLoaderClient* client = new TestModuleScriptLoaderClient;
  TestFetchURL(client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleScriptLoader unexpectedly finished synchronously.";
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();

  EXPECT_TRUE(client->WasNotifyFinished());
  EXPECT_TRUE(client->GetModuleScript());
}

TEST_F(ModuleScriptLoaderTest, FetchURL_OnWorklet) {
  InitializeForWorklet();
  TestModuleScriptLoaderClient* client = new TestModuleScriptLoaderClient;
  TestFetchURL(client);

  EXPECT_FALSE(client->WasNotifyFinished())
      << "ModuleScriptLoader unexpectedly finished synchronously.";

  // Advance until WorkletModuleScriptFetcher finishes looking up a cache in
  // WorkletModuleResponsesMap and issues a fetch request so that
  // ServeAsynchronousRequests() can serve for the pending request.
  platform_->RunUntilIdle();
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();

  EXPECT_TRUE(client->WasNotifyFinished());
  EXPECT_TRUE(client->GetModuleScript());
}

}  // namespace blink
