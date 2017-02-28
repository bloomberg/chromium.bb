// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSharedWorkerReportingProxyImpl_h
#define WebSharedWorkerReportingProxyImpl_h

#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/WorkerReportingProxy.h"
#include "platform/heap/GarbageCollected.h"

namespace blink {

class WebSharedWorkerImpl;

// An implementation of WorkerReportingProxy for SharedWorker. This is created
// and owned by WebSharedWorkerImpl on the main thread, accessed from a worker
// thread, and destroyed on the main thread.
class WebSharedWorkerReportingProxyImpl final
    : public GarbageCollectedFinalized<WebSharedWorkerReportingProxyImpl>,
      public WorkerReportingProxy {
  WTF_MAKE_NONCOPYABLE(WebSharedWorkerReportingProxyImpl);

 public:
  WebSharedWorkerReportingProxyImpl(WebSharedWorkerImpl*,
                                    ParentFrameTaskRunners*);
  ~WebSharedWorkerReportingProxyImpl() override;

  // WorkerReportingProxy methods:
  void countFeature(UseCounter::Feature) override;
  void countDeprecation(UseCounter::Feature) override;
  void reportException(const WTF::String&,
                       std::unique_ptr<SourceLocation>,
                       int exceptionId) override;
  void reportConsoleMessage(MessageSource,
                            MessageLevel,
                            const String& message,
                            SourceLocation*) override;
  void postMessageToPageInspector(const WTF::String&) override;
  void didEvaluateWorkerScript(bool success) override {}
  void didCloseWorkerGlobalScope() override;
  void willDestroyWorkerGlobalScope() override {}
  void didTerminateWorkerThread() override;

  DECLARE_TRACE();

 private:
  // Not owned because this outlives the reporting proxy.
  WebSharedWorkerImpl* m_worker;

  Member<ParentFrameTaskRunners> m_parentFrameTaskRunners;
};

}  // namespace blink

#endif  // WebSharedWorkerReportingProxyImpl_h
