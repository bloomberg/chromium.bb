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

class TestSingleModuleClient final
    : public GarbageCollectedFinalized<TestSingleModuleClient>,
      public SingleModuleClient {
  USING_GARBAGE_COLLECTED_MIXIN(TestSingleModuleClient);

 public:
  TestSingleModuleClient() = default;
  virtual ~TestSingleModuleClient() {}

  DEFINE_INLINE_TRACE() { visitor->trace(m_moduleScript); }

  void notifyModuleLoadFinished(ModuleScript* moduleScript) override {
    m_wasNotifyFinished = true;
    m_moduleScript = moduleScript;
  }

  bool wasNotifyFinished() const { return m_wasNotifyFinished; }
  ModuleScript* moduleScript() { return m_moduleScript; }

 private:
  bool m_wasNotifyFinished = false;
  Member<ModuleScript> m_moduleScript;
};

class TestScriptModuleResolver final : public ScriptModuleResolver {
 public:
  TestScriptModuleResolver() {}

  int registerModuleScriptCallCount() const {
    return m_registerModuleScriptCallCount;
  }

  void registerModuleScript(ModuleScript*) override {
    m_registerModuleScriptCallCount++;
  }

  ScriptModule resolve(const String& specifier,
                       const ScriptModule& referrer,
                       ExceptionState&) override {
    NOTREACHED();
    return ScriptModule();
  }

 private:
  int m_registerModuleScriptCallCount = 0;
};

}  // namespace

class ModuleMapTestModulator final : public DummyModulator {
 public:
  ModuleMapTestModulator();
  virtual ~ModuleMapTestModulator() {}

  DECLARE_TRACE();

  TestScriptModuleResolver* testScriptModuleResolver() {
    return m_resolver.get();
  }
  void resolveFetches();

 private:
  // Implements Modulator:

  ScriptModuleResolver* scriptModuleResolver() override {
    return m_resolver.get();
  }

  WebTaskRunner* taskRunner() override {
    return Platform::current()->currentThread()->getWebTaskRunner();
  };

  void fetchNewSingleModule(const ModuleScriptFetchRequest&,
                            ModuleGraphLevel,
                            ModuleScriptLoaderClient*) override;

  struct TestRequest : public GarbageCollectedFinalized<TestRequest> {
    KURL url;
    String nonce;
    Member<ModuleScriptLoaderClient> client;

    DEFINE_INLINE_TRACE() { visitor->trace(client); }
  };
  HeapVector<Member<TestRequest>> m_testRequests;

  Member<TestScriptModuleResolver> m_resolver;
};

ModuleMapTestModulator::ModuleMapTestModulator()
    : m_resolver(new TestScriptModuleResolver) {}

DEFINE_TRACE(ModuleMapTestModulator) {
  visitor->trace(m_testRequests);
  visitor->trace(m_resolver);
  DummyModulator::trace(visitor);
}

void ModuleMapTestModulator::fetchNewSingleModule(
    const ModuleScriptFetchRequest& request,
    ModuleGraphLevel,
    ModuleScriptLoaderClient* client) {
  TestRequest* testRequest = new TestRequest;
  testRequest->url = request.url();
  testRequest->nonce = request.nonce();
  testRequest->client = client;
  m_testRequests.push_back(testRequest);
}

void ModuleMapTestModulator::resolveFetches() {
  for (const auto& testRequest : m_testRequests) {
    ModuleScript* moduleScript = ModuleScript::create(
        ScriptModule(), testRequest->url, testRequest->nonce, ParserInserted,
        WebURLRequest::FetchCredentialsModeOmit);
    taskRunner()->postTask(
        BLINK_FROM_HERE,
        WTF::bind(&ModuleScriptLoaderClient::notifyNewSingleModuleFinished,
                  wrapPersistent(testRequest->client.get()),
                  wrapPersistent(moduleScript)));
  }
  m_testRequests.clear();
}

class ModuleMapTest : public testing::Test {
 public:
  void SetUp() override;

  ModuleMapTestModulator* modulator() { return m_modulator.get(); }
  ModuleMap* map() { return m_map; }

 protected:
  Persistent<ModuleMapTestModulator> m_modulator;
  Persistent<ModuleMap> m_map;
};

void ModuleMapTest::SetUp() {
  m_modulator = new ModuleMapTestModulator();
  m_map = ModuleMap::create(m_modulator.get());
}

TEST_F(ModuleMapTest, sequentialRequests) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;
  platform->advanceClockSeconds(1.);  // For non-zero DocumentParserTimings

  KURL url(KURL(), "https://example.com/foo.js");
  ModuleScriptFetchRequest moduleRequest(
      url, String(), ParserInserted, WebURLRequest::FetchCredentialsModeOmit);

  // First request
  TestSingleModuleClient* client = new TestSingleModuleClient;
  map()->fetchSingleModuleScript(moduleRequest,
                                 ModuleGraphLevel::TopLevelModuleFetch, client);
  modulator()->resolveFetches();
  EXPECT_FALSE(client->wasNotifyFinished())
      << "fetchSingleModuleScript shouldn't complete synchronously";
  platform->runUntilIdle();

  EXPECT_EQ(
      modulator()->testScriptModuleResolver()->registerModuleScriptCallCount(),
      1);
  EXPECT_TRUE(client->wasNotifyFinished());
  EXPECT_TRUE(client->moduleScript());
  EXPECT_EQ(client->moduleScript()->instantiationState(),
            ModuleInstantiationState::Uninstantiated);

  // Secondary request
  TestSingleModuleClient* client2 = new TestSingleModuleClient;
  map()->fetchSingleModuleScript(
      moduleRequest, ModuleGraphLevel::TopLevelModuleFetch, client2);
  modulator()->resolveFetches();
  EXPECT_FALSE(client2->wasNotifyFinished())
      << "fetchSingleModuleScript shouldn't complete synchronously";
  platform->runUntilIdle();

  EXPECT_EQ(
      modulator()->testScriptModuleResolver()->registerModuleScriptCallCount(),
      1)
      << "registerModuleScript sholudn't be called in secondary request.";
  EXPECT_TRUE(client2->wasNotifyFinished());
  EXPECT_TRUE(client2->moduleScript());
  EXPECT_EQ(client2->moduleScript()->instantiationState(),
            ModuleInstantiationState::Uninstantiated);
}

TEST_F(ModuleMapTest, concurrentRequestsShouldJoin) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;
  platform->advanceClockSeconds(1.);  // For non-zero DocumentParserTimings

  KURL url(KURL(), "https://example.com/foo.js");
  ModuleScriptFetchRequest moduleRequest(
      url, String(), ParserInserted, WebURLRequest::FetchCredentialsModeOmit);

  // First request
  TestSingleModuleClient* client = new TestSingleModuleClient;
  map()->fetchSingleModuleScript(moduleRequest,
                                 ModuleGraphLevel::TopLevelModuleFetch, client);

  // Secondary request (which should join the first request)
  TestSingleModuleClient* client2 = new TestSingleModuleClient;
  map()->fetchSingleModuleScript(
      moduleRequest, ModuleGraphLevel::TopLevelModuleFetch, client2);

  modulator()->resolveFetches();
  EXPECT_FALSE(client->wasNotifyFinished())
      << "fetchSingleModuleScript shouldn't complete synchronously";
  EXPECT_FALSE(client2->wasNotifyFinished())
      << "fetchSingleModuleScript shouldn't complete synchronously";
  platform->runUntilIdle();

  EXPECT_EQ(
      modulator()->testScriptModuleResolver()->registerModuleScriptCallCount(),
      1);

  EXPECT_TRUE(client->wasNotifyFinished());
  EXPECT_TRUE(client->moduleScript());
  EXPECT_EQ(client->moduleScript()->instantiationState(),
            ModuleInstantiationState::Uninstantiated);
  EXPECT_TRUE(client2->wasNotifyFinished());
  EXPECT_TRUE(client2->moduleScript());
  EXPECT_EQ(client2->moduleScript()->instantiationState(),
            ModuleInstantiationState::Uninstantiated);
}

}  // namespace blink
