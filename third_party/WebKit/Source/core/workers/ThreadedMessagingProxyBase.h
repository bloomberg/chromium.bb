// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadedMessagingProxyBase_h
#define ThreadedMessagingProxyBase_h

#include "core/CoreExport.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleTypes.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/WorkerClients.h"
#include "platform/wtf/Forward.h"

namespace blink {

class ExecutionContext;
class SourceLocation;
class ThreadableLoadingContext;
class WorkerInspectorProxy;
class WorkerThread;
class WorkerThreadStartupData;

// The base proxy class to talk to Worker/WorkletGlobalScope on a worker thread
// from the parent context thread (Note that this is always the main thread for
// now because nested worker is not supported yet).
//
// This must be created, accessed and destroyed on the parent context thread.
class CORE_EXPORT ThreadedMessagingProxyBase {
 public:
  void TerminateGlobalScope();

  // This method should be called in the destructor of the object which
  // initially created it. This object could either be a Worker or a Worklet.
  virtual void ParentObjectDestroyed();

  void CountFeature(WebFeature);
  void CountDeprecation(WebFeature);

  void ReportConsoleMessage(MessageSource,
                            MessageLevel,
                            const String& message,
                            std::unique_ptr<SourceLocation>);
  void PostMessageToPageInspector(const String&);

  // 'virtual' for testing.
  virtual void WorkerThreadTerminated();

  // Number of live messaging proxies, used by leak detection.
  static int ProxyCount();

 protected:
  ThreadedMessagingProxyBase(ExecutionContext*, WorkerClients*);
  virtual ~ThreadedMessagingProxyBase();

  void InitializeWorkerThread(std::unique_ptr<WorkerThreadStartupData>,
                              const KURL& script_url);
  virtual void WorkerThreadCreated();

  ThreadableLoadingContext* CreateThreadableLoadingContext() const;

  ExecutionContext* GetExecutionContext() const;
  ParentFrameTaskRunners* GetParentFrameTaskRunners() const;
  WorkerInspectorProxy* GetWorkerInspectorProxy() const;
  WorkerThread* GetWorkerThread() const;

  bool AskedToTerminate() const { return asked_to_terminate_; }

  // Transfers ownership of the clients to the caller.
  WorkerClients* ReleaseWorkerClients();

  // Returns true if this is called on the parent context thread.
  bool IsParentContextThread() const;

 private:
  virtual std::unique_ptr<WorkerThread> CreateWorkerThread(
      double origin_time) = 0;

  void ParentObjectDestroyedInternal();

  Persistent<ExecutionContext> execution_context_;
  Persistent<WorkerClients> worker_clients_;
  Persistent<WorkerInspectorProxy> worker_inspector_proxy_;

  // Accessed cross-thread when worker thread posts tasks to the parent.
  CrossThreadPersistent<ParentFrameTaskRunners> parent_frame_task_runners_;

  std::unique_ptr<WorkerThread> worker_thread_;

  bool may_be_destroyed_;
  bool asked_to_terminate_;
};

}  // namespace blink

#endif  // ThreadedMessagingProxyBase_h
