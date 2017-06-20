// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadedObjectProxyBase_h
#define ThreadedObjectProxyBase_h

#include "bindings/core/v8/SourceLocation.h"
#include "core/CoreExport.h"
#include "core/dom/MessagePort.h"
#include "core/workers/WorkerReportingProxy.h"

namespace blink {

class ParentFrameTaskRunners;
class ThreadedMessagingProxyBase;

// A proxy to talk to the parent object. This object is created and destroyed on
// the main thread, and used on the worker thread for proxying messages to the
// ThreadedMessagingProxyBase on the main thread. ThreadedMessagingProxyBase
// always outlives this proxy.
class CORE_EXPORT ThreadedObjectProxyBase : public WorkerReportingProxy {
  USING_FAST_MALLOC(ThreadedObjectProxyBase);
  WTF_MAKE_NONCOPYABLE(ThreadedObjectProxyBase);

 public:
  ~ThreadedObjectProxyBase() override = default;

  void ReportPendingActivity(bool has_pending_activity);

  // WorkerReportingProxy overrides.
  void CountFeature(WebFeature) override;
  void CountDeprecation(WebFeature) override;
  void ReportConsoleMessage(MessageSource,
                            MessageLevel,
                            const String& message,
                            SourceLocation*) override;
  void PostMessageToPageInspector(int session_id, const String&) override;
  void DidCloseWorkerGlobalScope() override;
  void DidTerminateWorkerThread() override;

 protected:
  explicit ThreadedObjectProxyBase(ParentFrameTaskRunners*);
  virtual CrossThreadWeakPersistent<ThreadedMessagingProxyBase>
  MessagingProxyWeakPtr() = 0;
  ParentFrameTaskRunners* GetParentFrameTaskRunners();

 private:
  // Used to post a task to ThreadedMessagingProxyBase on the parent context
  // thread.
  CrossThreadPersistent<ParentFrameTaskRunners> parent_frame_task_runners_;
};

}  // namespace blink

#endif  // ThreadedObjectProxyBase_h
