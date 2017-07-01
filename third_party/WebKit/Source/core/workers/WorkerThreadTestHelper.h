// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerThreadTestHelper_h
#define WorkerThreadTestHelper_h

#include <memory>

#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8CacheOptions.h"
#include "bindings/core/v8/V8GCController.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerGlobalScope.h"
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
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"
#include "public/platform/WebAddressSpace.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "v8/include/v8.h"

namespace blink {

class MockWorkerThreadLifecycleObserver final
    : public GarbageCollectedFinalized<MockWorkerThreadLifecycleObserver>,
      public WorkerThreadLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(MockWorkerThreadLifecycleObserver);
  WTF_MAKE_NONCOPYABLE(MockWorkerThreadLifecycleObserver);

 public:
  explicit MockWorkerThreadLifecycleObserver(
      WorkerThreadLifecycleContext* context)
      : WorkerThreadLifecycleObserver(context) {}

  MOCK_METHOD1(ContextDestroyed, void(WorkerThreadLifecycleContext*));
};

class WorkerThreadForTest : public WorkerThread {
 public:
  WorkerThreadForTest(ThreadableLoadingContext* loading_context,
                      WorkerReportingProxy& mock_worker_reporting_proxy)
      : WorkerThread(loading_context, mock_worker_reporting_proxy),
        worker_backing_thread_(
            WorkerBackingThread::CreateForTest("Test thread")) {}

  ~WorkerThreadForTest() override {}

  WorkerBackingThread& GetWorkerBackingThread() override {
    return *worker_backing_thread_;
  }
  void ClearWorkerBackingThread() override { worker_backing_thread_ = nullptr; }

  WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<WorkerThreadStartupData>) override;

  void StartWithSourceCode(SecurityOrigin* security_origin,
                           const String& source,
                           ParentFrameTaskRunners* parent_frame_task_runners) {
    std::unique_ptr<Vector<CSPHeaderAndType>> headers =
        WTF::MakeUnique<Vector<CSPHeaderAndType>>();
    CSPHeaderAndType header_and_type("contentSecurityPolicy",
                                     kContentSecurityPolicyHeaderTypeReport);
    headers->push_back(header_and_type);

    WorkerClients* clients = nullptr;

    Start(
        WorkerThreadStartupData::Create(
            KURL(kParsedURLString, "http://fake.url/"), "fake user agent",
            source, nullptr, kDontPauseWorkerGlobalScopeOnStart, headers.get(),
            "", security_origin, clients, kWebAddressSpaceLocal, nullptr,
            nullptr, WorkerV8Settings::Default()),
        parent_frame_task_runners);
  }

  void WaitForInit() {
    std::unique_ptr<WaitableEvent> completion_event =
        WTF::MakeUnique<WaitableEvent>();
    GetWorkerBackingThread().BackingThread().PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&WaitableEvent::Signal,
                        CrossThreadUnretained(completion_event.get())));
    completion_event->Wait();
  }

 private:
  std::unique_ptr<WorkerBackingThread> worker_backing_thread_;
};

class FakeWorkerGlobalScope : public WorkerGlobalScope {
 public:
  FakeWorkerGlobalScope(const KURL& url,
                        const String& user_agent,
                        WorkerThreadForTest* thread,
                        std::unique_ptr<SecurityOrigin::PrivilegeData>
                            starter_origin_privilege_data,
                        WorkerClients* worker_clients)
      : WorkerGlobalScope(url,
                          user_agent,
                          thread,
                          MonotonicallyIncreasingTime(),
                          std::move(starter_origin_privilege_data),
                          worker_clients) {}

  ~FakeWorkerGlobalScope() override {}

  // EventTarget
  const AtomicString& InterfaceName() const override {
    return EventTargetNames::DedicatedWorkerGlobalScope;
  }

  void ExceptionThrown(ErrorEvent*) override {}
};

inline WorkerOrWorkletGlobalScope* WorkerThreadForTest::CreateWorkerGlobalScope(
    std::unique_ptr<WorkerThreadStartupData> startup_data) {
  return new FakeWorkerGlobalScope(
      startup_data->script_url_, startup_data->user_agent_, this,
      std::move(startup_data->starter_origin_privilege_data_),
      std::move(startup_data->worker_clients_));
}

}  // namespace blink

#endif  // WorkerThreadTestHelper_h
