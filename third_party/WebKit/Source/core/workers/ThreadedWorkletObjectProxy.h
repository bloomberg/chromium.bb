// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadedWorkletObjectProxy_h
#define ThreadedWorkletObjectProxy_h

#include "bindings/core/v8/SourceLocation.h"
#include "core/CoreExport.h"
#include "core/dom/MessagePort.h"
#include "core/workers/ThreadedObjectProxyBase.h"
#include "core/workers/WorkerReportingProxy.h"

namespace blink {

class ThreadedWorkletMessagingProxy;
class WorkerThread;

// A proxy to talk to the parent worker object. See class comments on
// ThreadedObjectProxyBase.h for lifetime of this class etc.
class CORE_EXPORT ThreadedWorkletObjectProxy : public ThreadedObjectProxyBase {
  USING_FAST_MALLOC(ThreadedWorkletObjectProxy);
  WTF_MAKE_NONCOPYABLE(ThreadedWorkletObjectProxy);

 public:
  static std::unique_ptr<ThreadedWorkletObjectProxy> Create(
      ThreadedWorkletMessagingProxy*,
      ParentFrameTaskRunners*);
  ~ThreadedWorkletObjectProxy() override;

  void EvaluateScript(const String& source,
                      const KURL& script_url,
                      WorkerThread*);

  // ThreadedObjectProxyBase overrides.
  void ReportException(const String& error_message,
                       std::unique_ptr<SourceLocation>,
                       int exception_id) final {}
  void DidEvaluateWorkerScript(bool success) final {}
  void WillDestroyWorkerGlobalScope() final {}

 protected:
  ThreadedWorkletObjectProxy(ThreadedWorkletMessagingProxy*,
                             ParentFrameTaskRunners*);

  CrossThreadWeakPersistent<ThreadedMessagingProxyBase> MessagingProxyWeakPtr()
      final;

 private:
  // No guarantees about the lifetimes of tasks posted by this proxy wrt the
  // ThreadedWorkletMessagingProxy so a weak pointer must be used when posting
  // the tasks.
  CrossThreadWeakPersistent<ThreadedWorkletMessagingProxy>
      messaging_proxy_weak_ptr_;
};

}  // namespace blink

#endif  // ThreadedWorkletObjectProxy_h
