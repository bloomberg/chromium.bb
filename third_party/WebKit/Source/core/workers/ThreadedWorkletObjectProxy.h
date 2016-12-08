// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadedWorkletObjectProxy_h
#define ThreadedWorkletObjectProxy_h

#include "bindings/core/v8/SourceLocation.h"
#include "core/CoreExport.h"
#include "core/dom/MessagePort.h"
#include "core/workers/WorkerReportingProxy.h"

namespace blink {

class ThreadedWorkletMessagingProxy;

// A proxy to talk to the parent worklet object. This object is created and
// destroyed on the main thread, and used on the worklet thread for proxying
// messages to the ThreadedWorkletMessagingProxy on the main thread.
// ThreadedWorkletMessagingProxy always outlives this proxy.
class CORE_EXPORT ThreadedWorkletObjectProxy : public WorkerReportingProxy {
  USING_FAST_MALLOC(ThreadedWorkletObjectProxy);
  WTF_MAKE_NONCOPYABLE(ThreadedWorkletObjectProxy);

 public:
  static std::unique_ptr<ThreadedWorkletObjectProxy> create(
      const WeakPtr<ThreadedWorkletMessagingProxy>&,
      ParentFrameTaskRunners*);
  ~ThreadedWorkletObjectProxy() override;

  void reportPendingActivity(bool hasPendingActivity);

  // WorkerReportingProxy overrides.
  void countFeature(UseCounter::Feature) override;
  void countDeprecation(UseCounter::Feature) override;
  void reportException(const String& errorMessage,
                       std::unique_ptr<SourceLocation>,
                       int exceptionId) override {}
  void reportConsoleMessage(MessageSource,
                            MessageLevel,
                            const String& message,
                            SourceLocation*) override;
  void postMessageToPageInspector(const String&) override;
  ParentFrameTaskRunners* getParentFrameTaskRunners() override;
  void didEvaluateWorkerScript(bool success) override {}
  void didCloseWorkerGlobalScope() override;
  void willDestroyWorkerGlobalScope() override {}
  void didTerminateWorkerThread() override;

 protected:
  ThreadedWorkletObjectProxy(const WeakPtr<ThreadedWorkletMessagingProxy>&,
                             ParentFrameTaskRunners*);

 private:
  // No guarantees about the lifetimes of tasks posted by this proxy wrt the
  // ThreadedWorkletMessagingProxy so a weak pointer must be used when posting
  // the tasks.
  WeakPtr<ThreadedWorkletMessagingProxy> m_messagingProxyWeakPtr;

  // Used to post a task to ThreadedWorkletMessagingProxy on the parent context
  // thread.
  CrossThreadPersistent<ParentFrameTaskRunners> m_parentFrameTaskRunners;
};

}  // namespace blink

#endif  // ThreadedWorkletObjectProxy_h
