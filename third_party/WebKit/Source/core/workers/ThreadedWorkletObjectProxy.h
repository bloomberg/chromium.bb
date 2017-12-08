// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadedWorkletObjectProxy_h
#define ThreadedWorkletObjectProxy_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/workers/ThreadedObjectProxyBase.h"
#include "core/workers/WorkerReportingProxy.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

class ThreadedWorkletMessagingProxy;
class WorkletModuleResponsesMap;
class WorkletPendingTasks;
class WorkerThread;

// A proxy to talk to the parent worker object. See class comments on
// ThreadedObjectProxyBase.h for lifetime of this class etc.
// TODO(nhiroki): Consider merging this class into ThreadedObjectProxyBase
// after EvaluateScript() for classic script loading is removed in favor of
// module script loading.
class CORE_EXPORT ThreadedWorkletObjectProxy : public ThreadedObjectProxyBase {
  USING_FAST_MALLOC(ThreadedWorkletObjectProxy);

 public:
  static std::unique_ptr<ThreadedWorkletObjectProxy> Create(
      ThreadedWorkletMessagingProxy*,
      ParentFrameTaskRunners*);
  ~ThreadedWorkletObjectProxy() override;

  void FetchAndInvokeScript(
      const KURL& module_url_record,
      WorkletModuleResponsesMap*,
      network::mojom::FetchCredentialsMode,
      scoped_refptr<WebTaskRunner> outside_settings_task_runner,
      WorkletPendingTasks*,
      WorkerThread*);

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
  DISALLOW_COPY_AND_ASSIGN(ThreadedWorkletObjectProxy);
};

}  // namespace blink

#endif  // ThreadedWorkletObjectProxy_h
