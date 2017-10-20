// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ModuleMap.h"

#include "core/dom/Document.h"
#include "core/dom/Modulator.h"
#include "core/dom/ModuleScript.h"
#include "core/dom/ScriptModuleResolver.h"
#include "core/loader/modulescript/ModuleScriptFetchRequest.h"
#include "core/loader/modulescript/ModuleScriptLoaderClient.h"
#include "core/testing/DummyModulator.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Handle.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/Platform.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class TestSingleModuleClient final : public SingleModuleClient {
 public:
  TestSingleModuleClient() = default;
  virtual ~TestSingleModuleClient() {}

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(module_script_);
    SingleModuleClient::Trace(visitor);
  }

  void NotifyModuleLoadFinished(ModuleScript* module_script) override {
    was_notify_finished_ = true;
    module_script_ = module_script;
  }

  bool WasNotifyFinished() const { return was_notify_finished_; }
  ModuleScript* GetModuleScript() { return module_script_; }

 private:
  bool was_notify_finished_ = false;
  Member<ModuleScript> module_script_;
};

class TestScriptModuleResolver final : public ScriptModuleResolver {
 public:
  TestScriptModuleResolver() {}

  int RegisterModuleScriptCallCount() const {
    return register_module_script_call_count_;
  }

  void RegisterModuleScript(ModuleScript*) override {
    register_module_script_call_count_++;
  }

  void UnregisterModuleScript(ModuleScript*) override {
    FAIL() << "UnregisterModuleScript shouldn't be called in ModuleMapTest";
  }

  ModuleScript* GetHostDefined(const ScriptModule&) const override {
    NOTREACHED();
    return nullptr;
  }

  ScriptModule Resolve(const String& specifier,
                       const ScriptModule& referrer,
                       ExceptionState&) override {
    NOTREACHED();
    return ScriptModule();
  }

 private:
  int register_module_script_call_count_ = 0;
};

}  // namespace

class ModuleMapTestModulator final : public DummyModulator {
 public:
  ModuleMapTestModulator();
  virtual ~ModuleMapTestModulator() {}

  void Trace(blink::Visitor*);

  TestScriptModuleResolver* GetTestScriptModuleResolver() {
    return resolver_.Get();
  }
  void ResolveFetches();

 private:
  // Implements Modulator:

  ScriptModuleResolver* GetScriptModuleResolver() override {
    return resolver_.Get();
  }

  WebTaskRunner* TaskRunner() override {
    return Platform::Current()->CurrentThread()->GetWebTaskRunner();
  };

  void FetchNewSingleModule(const ModuleScriptFetchRequest&,
                            ModuleGraphLevel,
                            ModuleScriptLoaderClient*) override;

  struct TestRequest : public GarbageCollectedFinalized<TestRequest> {
    KURL url;
    ScriptFetchOptions options;
    Member<ModuleScriptLoaderClient> client;

    void Trace(blink::Visitor* visitor) { visitor->Trace(client); }
  };
  HeapVector<Member<TestRequest>> test_requests_;

  Member<TestScriptModuleResolver> resolver_;
};

ModuleMapTestModulator::ModuleMapTestModulator()
    : resolver_(new TestScriptModuleResolver) {}

void ModuleMapTestModulator::Trace(blink::Visitor* visitor) {
  visitor->Trace(test_requests_);
  visitor->Trace(resolver_);
  DummyModulator::Trace(visitor);
}

void ModuleMapTestModulator::FetchNewSingleModule(
    const ModuleScriptFetchRequest& request,
    ModuleGraphLevel,
    ModuleScriptLoaderClient* client) {
  TestRequest* test_request = new TestRequest;
  test_request->url = request.Url();
  test_request->options = request.Options();
  test_request->client = client;
  test_requests_.push_back(test_request);
}

void ModuleMapTestModulator::ResolveFetches() {
  for (const auto& test_request : test_requests_) {
    auto* module_script = ModuleScript::CreateForTest(
        this, ScriptModule(), test_request->url, test_request->options);
    TaskRunner()->PostTask(
        BLINK_FROM_HERE,
        WTF::Bind(&ModuleScriptLoaderClient::NotifyNewSingleModuleFinished,
                  WrapPersistent(test_request->client.Get()),
                  WrapPersistent(module_script)));
  }
  test_requests_.clear();
}

class ModuleMapTest : public ::testing::Test {
 public:
  void SetUp() override;

  ModuleMapTestModulator* Modulator() { return modulator_.Get(); }
  ModuleMap* Map() { return map_; }

 protected:
  Persistent<ModuleMapTestModulator> modulator_;
  Persistent<ModuleMap> map_;
};

void ModuleMapTest::SetUp() {
  modulator_ = new ModuleMapTestModulator();
  map_ = ModuleMap::Create(modulator_.Get());
}

TEST_F(ModuleMapTest, sequentialRequests) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;
  platform->AdvanceClockSeconds(1.);  // For non-zero DocumentParserTimings

  KURL url(NullURL(), "https://example.com/foo.js");
  ModuleScriptFetchRequest module_request(url, ScriptFetchOptions());

  // First request
  TestSingleModuleClient* client = new TestSingleModuleClient;
  Map()->FetchSingleModuleScript(
      module_request, ModuleGraphLevel::kTopLevelModuleFetch, client);
  Modulator()->ResolveFetches();
  EXPECT_FALSE(client->WasNotifyFinished())
      << "fetchSingleModuleScript shouldn't complete synchronously";
  platform->RunUntilIdle();

  EXPECT_EQ(Modulator()
                ->GetTestScriptModuleResolver()
                ->RegisterModuleScriptCallCount(),
            1);
  EXPECT_TRUE(client->WasNotifyFinished());
  EXPECT_TRUE(client->GetModuleScript());

  // Secondary request
  TestSingleModuleClient* client2 = new TestSingleModuleClient;
  Map()->FetchSingleModuleScript(
      module_request, ModuleGraphLevel::kTopLevelModuleFetch, client2);
  Modulator()->ResolveFetches();
  EXPECT_FALSE(client2->WasNotifyFinished())
      << "fetchSingleModuleScript shouldn't complete synchronously";
  platform->RunUntilIdle();

  EXPECT_EQ(Modulator()
                ->GetTestScriptModuleResolver()
                ->RegisterModuleScriptCallCount(),
            1)
      << "registerModuleScript sholudn't be called in secondary request.";
  EXPECT_TRUE(client2->WasNotifyFinished());
  EXPECT_TRUE(client2->GetModuleScript());
}

TEST_F(ModuleMapTest, concurrentRequestsShouldJoin) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;
  platform->AdvanceClockSeconds(1.);  // For non-zero DocumentParserTimings

  KURL url(NullURL(), "https://example.com/foo.js");
  ModuleScriptFetchRequest module_request(url, ScriptFetchOptions());

  // First request
  TestSingleModuleClient* client = new TestSingleModuleClient;
  Map()->FetchSingleModuleScript(
      module_request, ModuleGraphLevel::kTopLevelModuleFetch, client);

  // Secondary request (which should join the first request)
  TestSingleModuleClient* client2 = new TestSingleModuleClient;
  Map()->FetchSingleModuleScript(
      module_request, ModuleGraphLevel::kTopLevelModuleFetch, client2);

  Modulator()->ResolveFetches();
  EXPECT_FALSE(client->WasNotifyFinished())
      << "fetchSingleModuleScript shouldn't complete synchronously";
  EXPECT_FALSE(client2->WasNotifyFinished())
      << "fetchSingleModuleScript shouldn't complete synchronously";
  platform->RunUntilIdle();

  EXPECT_EQ(Modulator()
                ->GetTestScriptModuleResolver()
                ->RegisterModuleScriptCallCount(),
            1);

  EXPECT_TRUE(client->WasNotifyFinished());
  EXPECT_TRUE(client->GetModuleScript());
  EXPECT_TRUE(client2->WasNotifyFinished());
  EXPECT_TRUE(client2->GetModuleScript());
}

}  // namespace blink
