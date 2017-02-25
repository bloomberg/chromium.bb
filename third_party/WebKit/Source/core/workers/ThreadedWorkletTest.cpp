// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/ConsoleMessageStorage.h"
#include "core/testing/DummyPageHolder.h"
#include "core/workers/ThreadedWorkletGlobalScope.h"
#include "core/workers/ThreadedWorkletMessagingProxy.h"
#include "core/workers/ThreadedWorkletObjectProxy.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "core/workers/WorkerThread.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "core/workers/WorkerThreadTestHelper.h"
#include "core/workers/WorkletThreadHolder.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ThreadedWorkletThreadForTest : public WorkerThread {
 public:
  ThreadedWorkletThreadForTest(
      WorkerLoaderProxyProvider* workerLoaderProxyProvider,
      WorkerReportingProxy& workerReportingProxy)
      : WorkerThread(WorkerLoaderProxy::create(workerLoaderProxyProvider),
                     workerReportingProxy) {}
  ~ThreadedWorkletThreadForTest() override{};

  WorkerBackingThread& workerBackingThread() override {
    auto workletThreadHolder =
        WorkletThreadHolder<ThreadedWorkletThreadForTest>::getInstance();
    DCHECK(workletThreadHolder);
    return *workletThreadHolder->thread();
  }

  void clearWorkerBackingThread() override {}

  WorkerOrWorkletGlobalScope* createWorkerGlobalScope(
      std::unique_ptr<WorkerThreadStartupData> startupData) final {
    RefPtr<SecurityOrigin> securityOrigin =
        SecurityOrigin::create(startupData->m_scriptURL);
    return new ThreadedWorkletGlobalScope(
        startupData->m_scriptURL, startupData->m_userAgent,
        securityOrigin.release(), this->isolate(), this);
  }

  bool isOwningBackingThread() const final { return false; }

  static void ensureSharedBackingThread() {
    DCHECK(isMainThread());
    WorkletThreadHolder<ThreadedWorkletThreadForTest>::createForTest(
        "ThreadedWorkletThreadForTest");
  }

  static void clearSharedBackingThread() {
    DCHECK(isMainThread());
    WorkletThreadHolder<ThreadedWorkletThreadForTest>::clearInstance();
  }

  // Emulates API use on ThreadedWorkletGlobalScope.
  void countFeature(UseCounter::Feature feature) {
    EXPECT_TRUE(isCurrentThread());
    globalScope()->countFeature(feature);
    getParentFrameTaskRunners()
        ->get(TaskType::UnspecedTimer)
        ->postTask(BLINK_FROM_HERE, crossThreadBind(&testing::exitRunLoop));
  }

  // Emulates deprecated API use on ThreadedWorkletGlobalScope.
  void countDeprecation(UseCounter::Feature feature) {
    EXPECT_TRUE(isCurrentThread());
    EXPECT_EQ(0u, consoleMessageStorage()->size());
    globalScope()->countDeprecation(feature);

    // countDeprecation() should add a warning message.
    EXPECT_EQ(1u, consoleMessageStorage()->size());
    String consoleMessage = consoleMessageStorage()->at(0)->message();
    EXPECT_TRUE(consoleMessage.contains("deprecated"));

    getParentFrameTaskRunners()
        ->get(TaskType::UnspecedTimer)
        ->postTask(BLINK_FROM_HERE, crossThreadBind(&testing::exitRunLoop));
  }
};

class ThreadedWorkletMessagingProxyForTest
    : public ThreadedWorkletMessagingProxy {
 public:
  ThreadedWorkletMessagingProxyForTest(ExecutionContext* executionContext)
      : ThreadedWorkletMessagingProxy(executionContext) {
    m_mockWorkerLoaderProxyProvider =
        WTF::makeUnique<MockWorkerLoaderProxyProvider>();
    m_workerThread = WTF::makeUnique<ThreadedWorkletThreadForTest>(
        m_mockWorkerLoaderProxyProvider.get(), workletObjectProxy());
    ThreadedWorkletThreadForTest::ensureSharedBackingThread();
  }

  ~ThreadedWorkletMessagingProxyForTest() override {
    m_workerThread->workerLoaderProxy()->detachProvider(
        m_mockWorkerLoaderProxyProvider.get());
    m_workerThread->terminateAndWait();
    ThreadedWorkletThreadForTest::clearSharedBackingThread();
  };

  void start() {
    KURL scriptURL(ParsedURLString, "http://fake.url/");
    std::unique_ptr<Vector<char>> cachedMetaData = nullptr;
    Vector<CSPHeaderAndType> contentSecurityPolicyHeaders;
    String referrerPolicy = "";
    m_securityOrigin = SecurityOrigin::create(scriptURL);
    WorkerClients* workerClients = nullptr;
    Vector<String> originTrialTokens;
    std::unique_ptr<WorkerSettings> workerSettings = nullptr;
    m_workerThread->start(
        WorkerThreadStartupData::create(
            scriptURL, "fake user agent", "// fake source code",
            std::move(cachedMetaData), DontPauseWorkerGlobalScopeOnStart,
            &contentSecurityPolicyHeaders, referrerPolicy,
            m_securityOrigin.get(), workerClients, WebAddressSpaceLocal,
            &originTrialTokens, std::move(workerSettings),
            WorkerV8Settings::Default()),
        getParentFrameTaskRunners());
    workerInspectorProxy()->workerThreadCreated(
        toDocument(getExecutionContext()), m_workerThread.get(), scriptURL);
  }

 protected:
  std::unique_ptr<WorkerThread> createWorkerThread(double originTime) final {
    NOTREACHED();
    return nullptr;
  }

 private:
  friend class ThreadedWorkletTest;

  std::unique_ptr<MockWorkerLoaderProxyProvider>
      m_mockWorkerLoaderProxyProvider;
  RefPtr<SecurityOrigin> m_securityOrigin;
};

class ThreadedWorkletTest : public ::testing::Test {
 public:
  void SetUp() override {
    m_page = DummyPageHolder::create();
    m_messagingProxy = WTF::makeUnique<ThreadedWorkletMessagingProxyForTest>(
        &m_page->document());
  }

  ThreadedWorkletMessagingProxyForTest* messagingProxy() {
    return m_messagingProxy.get();
  }

  ThreadedWorkletThreadForTest* workerThread() {
    return static_cast<ThreadedWorkletThreadForTest*>(
        m_messagingProxy->workerThread());
  }

  Document& document() { return m_page->document(); }

 private:
  std::unique_ptr<DummyPageHolder> m_page;
  std::unique_ptr<ThreadedWorkletMessagingProxyForTest> m_messagingProxy;
};

TEST_F(ThreadedWorkletTest, UseCounter) {
  messagingProxy()->start();

  // This feature is randomly selected.
  const UseCounter::Feature feature1 = UseCounter::Feature::RequestFileSystem;

  // API use on the DedicatedWorkerGlobalScope should be recorded in UseCounter
  // on the Document.
  EXPECT_FALSE(UseCounter::isCounted(document(), feature1));
  workerThread()->postTask(
      BLINK_FROM_HERE,
      crossThreadBind(&ThreadedWorkletThreadForTest::countFeature,
                      crossThreadUnretained(workerThread()), feature1));
  testing::enterRunLoop();
  EXPECT_TRUE(UseCounter::isCounted(document(), feature1));

  // This feature is randomly selected from Deprecation::deprecationMessage().
  const UseCounter::Feature feature2 = UseCounter::Feature::PrefixedStorageInfo;

  // Deprecated API use on the ThreadedWorkletGlobalScope should be recorded in
  // UseCounter on the Document.
  EXPECT_FALSE(UseCounter::isCounted(document(), feature2));
  workerThread()->postTask(
      BLINK_FROM_HERE,
      crossThreadBind(&ThreadedWorkletThreadForTest::countDeprecation,
                      crossThreadUnretained(workerThread()), feature2));
  testing::enterRunLoop();
  EXPECT_TRUE(UseCounter::isCounted(document(), feature2));
}

}  // namespace blink
