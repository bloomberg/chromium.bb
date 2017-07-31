/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InProcessWorkerObjectProxy_h
#define InProcessWorkerObjectProxy_h

#include <memory>
#include "core/CoreExport.h"
#include "core/dom/MessagePort.h"
#include "core/workers/ThreadedObjectProxyBase.h"
#include "core/workers/WorkerReportingProxy.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/WeakPtr.h"

namespace blink {

class InProcessWorkerMessagingProxy;
class ParentFrameTaskRunners;
class ThreadedMessagingProxyBase;
class WorkerGlobalScope;
class WorkerOrWorkletGlobalScope;
class WorkerThread;

// A proxy to talk to the parent worker object. See class comments on
// ThreadedObjectProxyBase.h for lifetime of this class etc.
//
// This also checks pending activities on WorkerGlobalScope and reports a result
// to the message proxy when an exponential backoff timer is fired.
class CORE_EXPORT InProcessWorkerObjectProxy : public ThreadedObjectProxyBase {
  USING_FAST_MALLOC(InProcessWorkerObjectProxy);
  WTF_MAKE_NONCOPYABLE(InProcessWorkerObjectProxy);

 public:
  static std::unique_ptr<InProcessWorkerObjectProxy> Create(
      InProcessWorkerMessagingProxy*,
      ParentFrameTaskRunners*);
  ~InProcessWorkerObjectProxy() override;

  void PostMessageToWorkerObject(RefPtr<SerializedScriptValue>,
                                 MessagePortChannelArray);
  void ProcessUnhandledException(int exception_id, WorkerThread*);
  void ProcessMessageFromWorkerObject(RefPtr<SerializedScriptValue> message,
                                      MessagePortChannelArray channels,
                                      WorkerThread*);

  // ThreadedObjectProxyBase overrides.
  void ReportException(const String& error_message,
                       std::unique_ptr<SourceLocation>,
                       int exception_id) override;
  void DidCreateWorkerGlobalScope(WorkerOrWorkletGlobalScope*) override;
  void DidEvaluateWorkerScript(bool success) override;
  void WillDestroyWorkerGlobalScope() override;

 protected:
  InProcessWorkerObjectProxy(InProcessWorkerMessagingProxy*,
                             ParentFrameTaskRunners*);

  CrossThreadWeakPersistent<ThreadedMessagingProxyBase> MessagingProxyWeakPtr()
      final;

 private:
  friend class InProcessWorkerObjectProxyForTest;

  void StartPendingActivityTimer();
  void CheckPendingActivity(TimerBase*);

  // No guarantees about the lifetimes of tasks posted by this proxy wrt the
  // InProcessWorkerMessagingProxy so a weak pointer must be used when posting
  // the tasks.
  CrossThreadWeakPersistent<InProcessWorkerMessagingProxy>
      messaging_proxy_weak_ptr_;

  // Used for checking pending activities on the worker global scope. This is
  // cancelled when the worker global scope is destroyed.
  std::unique_ptr<TaskRunnerTimer<InProcessWorkerObjectProxy>> timer_;

  // The default interval duration of the timer. This is usually
  // kDefaultIntervalInSec but made as a member variable for testing.
  double default_interval_in_sec_;

  // The next interval duration of the timer. This is initially set to
  // |m_defaultIntervalInSec| and exponentially increased up to
  // |m_maxIntervalInSec|.
  double next_interval_in_sec_;

  // The max interval duration of the timer. This is usually kMaxIntervalInSec
  // but made as a member variable for testing.
  double max_interval_in_sec_;

  CrossThreadPersistent<WorkerGlobalScope> worker_global_scope_;
};

}  // namespace blink

#endif  // InProcessWorkerObjectProxy_h
