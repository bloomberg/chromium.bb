// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/modulescript/ModuleScriptLoader.h"

#include "core/dom/Document.h"
#include "core/dom/Modulator.h"
#include "core/dom/ModuleScript.h"
#include "core/loader/modulescript/ModuleScriptFetchRequest.h"
#include "core/loader/modulescript/ModuleScriptLoaderClient.h"
#include "core/loader/modulescript/ModuleScriptLoaderRegistry.h"
#include "core/testing/DummyModulator.h"
#include "core/testing/DummyPageHolder.h"
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

  DEFINE_INLINE_TRACE() { visitor->trace(m_moduleScript); }

  void notifyNewSingleModuleFinished(ModuleScript* moduleScript) override {
    m_wasNotifyFinished = true;
    m_moduleScript = moduleScript;
  }

  bool wasNotifyFinished() const { return m_wasNotifyFinished; }
  ModuleScript* moduleScript() { return m_moduleScript; }

 private:
  bool m_wasNotifyFinished = false;
  Member<ModuleScript> m_moduleScript;
};

class ModuleScriptLoaderTestModulator final : public DummyModulator {
 public:
  ModuleScriptLoaderTestModulator(RefPtr<ScriptState> scriptState,
                                  RefPtr<SecurityOrigin> securityOrigin)
      : m_scriptState(std::move(scriptState)),
        m_securityOrigin(std::move(securityOrigin)) {}

  ~ModuleScriptLoaderTestModulator() override {}

  SecurityOrigin* securityOrigin() override { return m_securityOrigin.get(); }

  ScriptModule compileModule(const String& script,
                             const String& urlStr) override {
    ScriptState::Scope scope(m_scriptState.get());
    return ScriptModule::compile(m_scriptState->isolate(),
                                 "export default 'foo';", "");
  }

  DECLARE_TRACE();

 private:
  RefPtr<ScriptState> m_scriptState;
  RefPtr<SecurityOrigin> m_securityOrigin;
};

DEFINE_TRACE(ModuleScriptLoaderTestModulator) {
  DummyModulator::trace(visitor);
}

}  // namespace

class ModuleScriptLoaderTest : public ::testing::Test {
  DISALLOW_COPY_AND_ASSIGN(ModuleScriptLoaderTest);

 public:
  ModuleScriptLoaderTest() = default;
  void SetUp() override;

  LocalFrame& frame() { return m_dummyPageHolder->frame(); }
  Document& document() { return m_dummyPageHolder->document(); }
  ResourceFetcher* fetcher() { return m_fetcher.get(); }
  Modulator* modulator() { return m_modulator.get(); }

 protected:
  ScopedTestingPlatformSupport<FetchTestingPlatformSupport> m_platform;
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
  Persistent<ResourceFetcher> m_fetcher;
  Persistent<Modulator> m_modulator;
};

void ModuleScriptLoaderTest::SetUp() {
  m_platform->advanceClockSeconds(1.);  // For non-zero DocumentParserTimings
  m_dummyPageHolder = DummyPageHolder::create(IntSize(500, 500));
  document().setURL(KURL(KURL(), "https://example.test"));
  m_fetcher = ResourceFetcher::create(
      MockFetchContext::create(MockFetchContext::kShouldLoadNewResource));
  m_modulator = new ModuleScriptLoaderTestModulator(
      ScriptState::forMainWorld(&frame()), document().getSecurityOrigin());
}

TEST_F(ModuleScriptLoaderTest, fetchDataURL) {
  ModuleScriptLoaderRegistry* registry = ModuleScriptLoaderRegistry::create();
  KURL url(KURL(), "data:text/javascript,export default 'grapes';");
  ModuleScriptFetchRequest moduleRequest(
      url, String(), ParserInserted, WebURLRequest::FetchCredentialsModeOmit);
  TestModuleScriptLoaderClient* client = new TestModuleScriptLoaderClient;
  registry->fetch(moduleRequest, ModuleGraphLevel::TopLevelModuleFetch,
                  modulator(), fetcher(), client);

  EXPECT_TRUE(client->wasNotifyFinished())
      << "ModuleScriptLoader should finish synchronously.";
  ASSERT_TRUE(client->moduleScript());
  EXPECT_EQ(client->moduleScript()->instantiationState(),
            ModuleInstantiationState::Uninstantiated);
}

TEST_F(ModuleScriptLoaderTest, fetchInvalidURL) {
  ModuleScriptLoaderRegistry* registry = ModuleScriptLoaderRegistry::create();
  KURL url;
  EXPECT_FALSE(url.isValid());
  ModuleScriptFetchRequest moduleRequest(
      url, String(), ParserInserted, WebURLRequest::FetchCredentialsModeOmit);
  TestModuleScriptLoaderClient* client = new TestModuleScriptLoaderClient;
  registry->fetch(moduleRequest, ModuleGraphLevel::TopLevelModuleFetch,
                  modulator(), fetcher(), client);

  EXPECT_TRUE(client->wasNotifyFinished())
      << "ModuleScriptLoader should finish synchronously.";
  EXPECT_FALSE(client->moduleScript());
}

TEST_F(ModuleScriptLoaderTest, fetchURL) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/module.js");
  URLTestHelpers::registerMockedURLLoad(
      url, testing::webTestDataPath("module.js"), "text/javascript");

  ModuleScriptLoaderRegistry* registry = ModuleScriptLoaderRegistry::create();
  ModuleScriptFetchRequest moduleRequest(
      url, String(), ParserInserted, WebURLRequest::FetchCredentialsModeOmit);
  TestModuleScriptLoaderClient* client = new TestModuleScriptLoaderClient;
  registry->fetch(moduleRequest, ModuleGraphLevel::TopLevelModuleFetch,
                  modulator(), fetcher(), client);

  EXPECT_FALSE(client->wasNotifyFinished())
      << "ModuleScriptLoader unexpectedly finished synchronously.";
  m_platform->getURLLoaderMockFactory()->serveAsynchronousRequests();

  EXPECT_TRUE(client->wasNotifyFinished());
  EXPECT_FALSE(client->moduleScript());
}

}  // namespace blink
