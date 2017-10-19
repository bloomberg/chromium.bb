// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SharedWorkerReportingProxy_h
#define SharedWorkerReportingProxy_h

#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/WorkerReportingProxy.h"
#include "platform/heap/GarbageCollected.h"

namespace blink {

class WebSharedWorkerImpl;

// An implementation of WorkerReportingProxy for SharedWorker. This is created
// and owned by WebSharedWorkerImpl on the main thread, accessed from a worker
// thread, and destroyed on the main thread.
class SharedWorkerReportingProxy final
    : public GarbageCollectedFinalized<SharedWorkerReportingProxy>,
      public WorkerReportingProxy {
  WTF_MAKE_NONCOPYABLE(SharedWorkerReportingProxy);

 public:
  SharedWorkerReportingProxy(WebSharedWorkerImpl*, ParentFrameTaskRunners*);
  ~SharedWorkerReportingProxy() override;

  // WorkerReportingProxy methods:
  void CountFeature(WebFeature) override;
  void CountDeprecation(WebFeature) override;
  void ReportException(const WTF::String&,
                       std::unique_ptr<SourceLocation>,
                       int exception_id) override;
  void ReportConsoleMessage(MessageSource,
                            MessageLevel,
                            const String& message,
                            SourceLocation*) override;
  void PostMessageToPageInspector(int session_id, const WTF::String&) override;
  void DidEvaluateWorkerScript(bool success) override {}
  void DidCloseWorkerGlobalScope() override;
  void WillDestroyWorkerGlobalScope() override {}
  void DidTerminateWorkerThread() override;

  void Trace(blink::Visitor*);

 private:
  // Not owned because this outlives the reporting proxy.
  WebSharedWorkerImpl* worker_;

  Member<ParentFrameTaskRunners> parent_frame_task_runners_;
};

}  // namespace blink

#endif  // SharedWorkerReportingProxy_h
