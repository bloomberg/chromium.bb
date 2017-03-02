// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8CacheOptions.h"
#include "bindings/core/v8/V8GCController.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThread.h"
#include "core/workers/WorkerThreadLifecycleObserver.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/heap/Handle.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebAddressSpace.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "v8/include/v8.h"
#include "wtf/CurrentTime.h"
#include "wtf/Forward.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"

namespace blink {

class MockWorkerLoaderProxyProvider : public WorkerLoaderProxyProvider {
 public:
  MockWorkerLoaderProxyProvider() {}
  ~MockWorkerLoaderProxyProvider() override {}

  void postTaskToLoader(const WebTraceLocation&,
                        std::unique_ptr<WTF::CrossThreadClosure>) override {
    NOTIMPLEMENTED();
  }

  void postTaskToWorkerGlobalScope(
      const WebTraceLocation&,
      std::unique_ptr<WTF::CrossThreadClosure>) override {
    NOTIMPLEMENTED();
  }

  ThreadableLoadingContext* getThreadableLoadingContext() override {
    NOTIMPLEMENTED();
    return nullptr;
  }
};

class MockWorkerReportingProxy : public WorkerReportingProxy {
 public:
  MockWorkerReportingProxy() {}
  ~MockWorkerReportingProxy() override {}

  MOCK_METHOD1(countFeature, void(UseCounter::Feature));
  MOCK_METHOD1(countDeprecation, void(UseCounter::Feature));
  MOCK_METHOD3(reportExceptionMock,
               void(const String& errorMessage,
                    SourceLocation*,
                    int exceptionId));
  MOCK_METHOD4(reportConsoleMessage,
               void(MessageSource,
                    MessageLevel,
                    const String& message,
                    SourceLocation*));
  MOCK_METHOD1(postMessageToPageInspector, void(const String&));

  MOCK_METHOD1(didCreateWorkerGlobalScope, void(WorkerOrWorkletGlobalScope*));
  MOCK_METHOD0(didInitializeWorkerContext, void());
  MOCK_METHOD2(willEvaluateWorkerScriptMock,
               void(size_t scriptSize, size_t cachedMetadataSize));
  MOCK_METHOD1(didEvaluateWorkerScript, void(bool success));
  MOCK_METHOD0(didCloseWorkerGlobalScope, void());
  MOCK_METHOD0(willDestroyWorkerGlobalScope, void());
  MOCK_METHOD0(didTerminateWorkerThread, void());

  void reportException(const String& errorMessage,
                       std::unique_ptr<SourceLocation> location,
                       int exceptionId) {
    reportExceptionMock(errorMessage, location.get(), exceptionId);
  }

  void willEvaluateWorkerScript(size_t scriptSize,
                                size_t cachedMetadataSize) override {
    m_scriptEvaluationEvent.signal();
    willEvaluateWorkerScriptMock(scriptSize, cachedMetadataSize);
  }

  void waitUntilScriptEvaluation() { m_scriptEvaluationEvent.wait(); }

 private:
  WaitableEvent m_scriptEvaluationEvent;
};

class MockWorkerThreadLifecycleObserver final
    : public GarbageCollectedFinalized<MockWorkerThreadLifecycleObserver>,
      public WorkerThreadLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(MockWorkerThreadLifecycleObserver);
  WTF_MAKE_NONCOPYABLE(MockWorkerThreadLifecycleObserver);

 public:
  explicit MockWorkerThreadLifecycleObserver(
      WorkerThreadLifecycleContext* context)
      : WorkerThreadLifecycleObserver(context) {}

  MOCK_METHOD1(contextDestroyed, void(WorkerThreadLifecycleContext*));
};

class WorkerThreadForTest : public WorkerThread {
 public:
  WorkerThreadForTest(WorkerLoaderProxyProvider* mockWorkerLoaderProxyProvider,
                      WorkerReportingProxy& mockWorkerReportingProxy)
      : WorkerThread(WorkerLoaderProxy::create(mockWorkerLoaderProxyProvider),
                     mockWorkerReportingProxy),
        m_workerBackingThread(
            WorkerBackingThread::createForTest("Test thread")) {}

  ~WorkerThreadForTest() override {}

  WorkerBackingThread& workerBackingThread() override {
    return *m_workerBackingThread;
  }
  void clearWorkerBackingThread() override { m_workerBackingThread = nullptr; }

  WorkerOrWorkletGlobalScope* createWorkerGlobalScope(
      std::unique_ptr<WorkerThreadStartupData>) override;

  void startWithSourceCode(SecurityOrigin* securityOrigin,
                           const String& source,
                           ParentFrameTaskRunners* parentFrameTaskRunners) {
    std::unique_ptr<Vector<CSPHeaderAndType>> headers =
        WTF::makeUnique<Vector<CSPHeaderAndType>>();
    CSPHeaderAndType headerAndType("contentSecurityPolicy",
                                   ContentSecurityPolicyHeaderTypeReport);
    headers->push_back(headerAndType);

    WorkerClients* clients = nullptr;

    start(WorkerThreadStartupData::create(
              KURL(ParsedURLString, "http://fake.url/"), "fake user agent",
              source, nullptr, DontPauseWorkerGlobalScopeOnStart, headers.get(),
              "", securityOrigin, clients, WebAddressSpaceLocal, nullptr,
              nullptr, WorkerV8Settings::Default()),
          parentFrameTaskRunners);
  }

  void waitForInit() {
    std::unique_ptr<WaitableEvent> completionEvent =
        WTF::makeUnique<WaitableEvent>();
    workerBackingThread().backingThread().postTask(
        BLINK_FROM_HERE,
        crossThreadBind(&WaitableEvent::signal,
                        crossThreadUnretained(completionEvent.get())));
    completionEvent->wait();
  }

 private:
  std::unique_ptr<WorkerBackingThread> m_workerBackingThread;
};

class FakeWorkerGlobalScope : public WorkerGlobalScope {
 public:
  FakeWorkerGlobalScope(
      const KURL& url,
      const String& userAgent,
      WorkerThreadForTest* thread,
      std::unique_ptr<SecurityOrigin::PrivilegeData> starterOriginPrivilegeData,
      WorkerClients* workerClients)
      : WorkerGlobalScope(url,
                          userAgent,
                          thread,
                          monotonicallyIncreasingTime(),
                          std::move(starterOriginPrivilegeData),
                          workerClients) {}

  ~FakeWorkerGlobalScope() override {}

  // EventTarget
  const AtomicString& interfaceName() const override {
    return EventTargetNames::DedicatedWorkerGlobalScope;
  }

  void exceptionThrown(ErrorEvent*) override {}
};

inline WorkerOrWorkletGlobalScope* WorkerThreadForTest::createWorkerGlobalScope(
    std::unique_ptr<WorkerThreadStartupData> startupData) {
  return new FakeWorkerGlobalScope(
      startupData->m_scriptURL, startupData->m_userAgent, this,
      std::move(startupData->m_starterOriginPrivilegeData),
      std::move(startupData->m_workerClients));
}

}  // namespace blink
